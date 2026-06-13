# textDocument/completion

## 현황

| 항목 | 상태 |
|------|------|
| LSP 요청 전송 | ✅ `LspService::completion()` |
| 응답 파싱 | ✅ `parse_completion()` → `std::vector<LspCompletionItem>` |
| 결과 저장 | ✅ `set_completion()` / `completion_items()` |
| UI 렌더링 | ✅ `render_completion()` — 최대 10개 목록 표시 |
| 트리거 | ❌ 없음 (키 바인딩도, 자동 트리거도 없음) |

## 문제

파이프라인 전체가 구현돼 있지만 트리거가 없어 실질적으로 사용 불가합니다.

```
LspService::completion()    ← 호출할 경로 없음
set_completion()            ← 저장 가능
render_completion()         ← 렌더 가능하지만 completion_items_ 항상 비어 있음
```

## 설계

### 트리거 방식 결정

두 가지 방식이 있으며 동시에 구현 가능:

**방식 A — 자동 트리거 (Auto-complete)**
Insert 모드에서 인쇄 가능 문자 입력 후 자동으로 completion 요청.
- 장점: 사용자가 아무 키도 안 눌러도 됨
- 단점: 모든 키 입력마다 LSP 요청 → debounce 필요 (100~200ms)
- debounce 없으면 LSP 응답이 밀려 overlay 깜빡임

**방식 B — 명시적 키 트리거 (권장, 구현 간단)**
Insert 모드에서 `Ctrl-N` 또는 `Ctrl-Space`로 수동 트리거.
- 장점: 구현 단순, LSP 부하 없음
- 단점: 사용자가 키를 눌러야 함

**권장: 방식 B 먼저, 이후 방식 A 추가**

### 1. Insert 모드 키 매핑

```cpp
// InputTranslator.hpp — Insert 모드 분기에 추가
if (event == ftxui::Event::Special("\x0e"))  // Ctrl-N
    return core::Command::lsp_completion;
```

### 2. InputDispatcher 처리

```cpp
// dispatch_insert() 또는 공통 경로에서
case Command::lsp_completion: {
    auto pos = doc.position();
    lsp_.completion(uri_, pos.line, pos.col, [&lsp=lsp_](auto items) {
        lsp.set_completion(std::move(items));
    });
    return mode_;
}
```

### 3. 완성 항목 선택

현재 `render_completion()`은 목록만 표시하고 선택 UI가 없습니다.
선택 입력 처리를 위한 상태:

```
Insert 모드 + completion overlay 표시 중:
  Ctrl-N / Tab  → 다음 항목으로 커서 이동
  Ctrl-P        → 이전 항목
  Enter         → 선택한 항목 삽입
  Escape / 다른 키 → overlay 닫기, 원래 Insert 모드 입력 처리
```

항목 삽입 로직:
- 커서 위치에서 현재 단어(identifier prefix)를 삭제
- 선택한 `LspCompletionItem::insert_text` (또는 `label`) 삽입
- `insertTextFormat == Snippet`이면 snippet placeholder 처리 (선택적, 복잡도 높음)

### 4. render_completion() 개선

현재 구현이 이미 목록을 렌더링하므로 선택 커서 표시만 추가:
```
╭── Completions ─────────────────╮
│ ▶ std::string                  │  ← 선택된 항목 강조
│   std::string_view             │
│   std::stringstream            │
│   ... 7 more                   │
╰────────────────────────────────╯
```

## 영향 파일

| 파일 | 변경 |
|------|------|
| `src/core/interfaces/Command.hpp` | `lsp_completion` 커맨드 (이미 있을 수 있음) |
| `src/adapters/InputTranslator.hpp` | Ctrl-N → `lsp_completion` (Insert 모드) |
| `src/core/usecases/InputDispatcher.hpp` | completion 트리거 + 선택 상태 |
| `src/drivers/FtxuiRenderer.cpp` | `render_completion()` 선택 커서 추가 |
| `tests/unit/test_InputTranslator.cpp` | Ctrl-N 매핑 테스트 |
| `tests/unit/test_EditorCommands.cpp` | completion 트리거 흐름 테스트 |

## 테스트 시나리오

- Insert 모드에서 `Ctrl-N` → completion 요청 발생
- 결과 수신 후 overlay 표시
- `Ctrl-N` 반복 → 다음 항목 선택
- Enter → 선택 항목 Buffer에 삽입
- Escape → overlay 닫힘, Insert 모드 유지
- 결과 없음 → overlay 표시 안 함

## 전제 조건

없음 (독립 구현 가능)

단, 자동 트리거(방식 A) 구현 시 debounce 타이머 인프라 필요.
