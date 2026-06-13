# textDocument/rename

## 현황

| 항목 | 상태 |
|------|------|
| LSP 요청 전송 | ✅ `LspService::rename()` 구현됨 |
| 응답 파싱 | ⚠️ raw JSON callback — 적용 안 됨 |
| Buffer 반영 | ❌ 없음 |
| 입력 프롬프트 UI | ❌ 없음 |
| 키 바인딩 | ⚠️ `<Space>rn` — 명시적 no-op |

## 문제

두 가지가 동시에 없습니다:

1. **새 이름 입력 UI** — `rename(uri, line, col, new_name, cb)` 호출에 `new_name`이 필요한데, 받을 방법이 없음. 빈 문자열을 보내면 clangd가 심볼을 망가뜨리므로 현재 명시적 no-op으로 막혀 있음.

2. **`WorkspaceEdit` 적용 로직** — clangd는 여러 파일에 걸친 편집을 `WorkspaceEdit` (파일별 `TextEdit[]` 맵)으로 응답. Buffer에 적용할 코드 없음.

```
<Space>rn
  └─ InputDispatcher::AfterSpaceR (no-op, 주석으로 설명됨)
       └─ (아무것도 안 함)
```

## 설계

### 의존 관계

```
rename
 ├─ Buffer::apply_text_edits()       ← formatting과 공유 (formatting.md 참조)
 └─ Prompt 모드 (새 이름 입력)       ← signature-help.md, completion.md와 독립
```

### 1. Prompt 모드

단일 라인 텍스트 입력을 받는 모달 상태. 두 가지 구현 방식:

**방식 A — 새 EditorMode 추가 (권장)**
```cpp
// EditorMode.hpp
enum class EditorMode { Normal, Insert, Prompt };
```
- `Prompt` 모드에서 InputTranslator는 모든 인쇄 가능 문자를 `insert_char`로 통과
- `Enter` → 프롬프트 완료 → 콜백 호출 후 Normal 복귀
- `Escape` → 프롬프트 취소 → Normal 복귀
- FtxuiRenderer: 상태바에 `Rename: <입력중인이름>_` 형태로 표시

**방식 B — InputDispatcher 오버레이 상태**
- 기존 모드 enum 변경 없이 dispatcher 내부 상태로만 관리
- 복잡성은 낮지만 렌더러가 프롬프트 상태를 알기 어려움

### 2. `WorkspaceEdit` 적용

clangd rename 응답 구조:
```json
{
  "changes": {
    "file:///path/to/file.cpp": [ TextEdit, ... ],
    "file:///path/to/other.hpp": [ TextEdit, ... ]
  }
}
```

현재 에디터는 단일 파일만 열어둠. 구현 전략:

- `changes` 맵에서 현재 URI에 해당하는 `TextEdit[]`만 추출
- `Buffer::apply_text_edits()` 로 역순 적용 (formatting과 동일)
- 다른 파일의 편집은 현재 스코프 밖 (단일 파일 에디터)

```cpp
// InputDispatcher rename 콜백 (Prompt 완료 후)
lsp_.rename(uri_, pos.line, pos.col, new_name, [&doc, uri=uri_](const nlohmann::json& ws_edit) {
    if (!ws_edit.contains("changes")) return;
    const auto& changes = ws_edit["changes"];
    if (!changes.contains(uri)) return;
    auto edits = LspService::parse_text_edits(changes[uri]);
    std::sort(edits.rbegin(), edits.rend(), by_position);
    for (const auto& e : edits)
        doc.buffer().apply_text_edit(e.start_line, e.start_col,
                                     e.end_line, e.end_col, e.new_text);
    doc.cursor().clamp();
});
```

### 3. 입력 흐름

```
Normal 모드에서 <Space>rn
  → InputDispatcher: Prompt 모드 진입, "Rename: " 프롬프트 시작
  → 사용자 입력 (문자 타이핑, Backspace)
  → Enter: new_name 확정 → lsp_.rename() 호출 → WorkspaceEdit 수신 → apply
  → Escape: 취소, Normal 복귀
```

## 영향 파일

| 파일 | 변경 |
|------|------|
| `src/core/usecases/EditorMode.hpp` | `Prompt` 모드 추가 |
| `src/core/usecases/InputDispatcher.hpp` | AfterSpaceR에서 Prompt 진입, 입력 처리 |
| `src/adapters/InputTranslator.hpp` | Prompt 모드 키 매핑 추가 |
| `src/core/usecases/LspService.hpp` | `parse_text_edits()` static helper |
| `src/core/usecases/LspService.cpp` | `parse_text_edits()` 구현 |
| `src/core/entities/Buffer.hpp/.cpp` | `apply_text_edit()` (formatting과 공유) |
| `src/drivers/FtxuiRenderer.cpp` | Prompt 모드 상태바 렌더링 |
| `tests/unit/test_EditorCommands.cpp` | Prompt 모드 입력 흐름 테스트 |

## 테스트 시나리오

- `<Space>rn` → Prompt 진입, 상태바에 "Rename: " 표시
- 타이핑 → 상태바 실시간 업데이트
- Backspace → 마지막 문자 삭제
- Escape → Normal 복귀, LSP 요청 안 보냄
- Enter (빈 입력) → 요청 안 보냄 (빈 이름 방지)
- Enter (유효한 이름) → lsp_.rename() 호출, 응답 후 Buffer 수정
- 실제 clangd: 심볼 rename 후 현재 파일의 모든 참조가 업데이트됨

## 전제 조건

- `Buffer::apply_text_edits()` (formatting.md와 공유)
- Prompt 모드 UI (이 피처 자체가 최초 도입)
