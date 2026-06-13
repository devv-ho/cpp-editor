# textDocument/formatting

## 현황

| 항목 | 상태 |
|------|------|
| LSP 요청 전송 | ✅ `LspService::formatting()` |
| 응답 파싱 | ⚠️ raw JSON callback — 적용 안 됨 |
| Buffer 반영 | ❌ 없음 |
| UI 피드백 | ❌ 없음 |
| 키 바인딩 | ✅ `<Space>f` |

## 문제

`InputDispatcher`에서 `<Space>f`를 누르면 `LspService::formatting(uri, cb)` 호출까지는 됩니다.
clangd는 `TextEdit[]` 배열을 응답으로 보내지만, 콜백 `cb`가 해당 JSON을 받아도
Buffer에 적용하는 코드가 없어 결과가 완전히 무시됩니다.

```
<Space>f
  └─ InputDispatcher::AfterSpaceF
       └─ lsp_.formatting(uri_, [](json edits){ /* 무시 */ })
```

## 설계

### 1. `Buffer::apply_text_edits()`

LSP `TextEdit` 구조:
```json
{ "range": { "start": {"line":0,"character":4}, "end": {"line":0,"character":8} },
  "newText": "void" }
```

Buffer에 다음 메서드 추가 (`src/core/entities/Buffer.hpp`):
```cpp
// LSP TextEdit 한 건을 Buffer에 적용.
// 반드시 역순(end line 내림차순)으로 호출해야 선행 편집이 후행 offset을 오염시키지 않음.
std::expected<void, BufferError>
apply_text_edit(std::size_t start_line, std::size_t start_col,
                std::size_t end_line,   std::size_t end_col,
                std::string_view new_text);
```

복수 편집 적용 시 **역순** 정렬 필수 (line desc → col desc).
내부 구현은 범위 내 텍스트를 제거하고 `new_text`를 삽입하는 방식.

### 2. `LspService::formatting()` 콜백 수정

`src/core/usecases/LspService.cpp`:
- 현재 `FormattingCallback cb`는 raw JSON을 외부로 전달
- 콜백 내부에서 `parse_text_edits(json)` → 역순 정렬 → `doc_.buffer().apply_text_edit()` 호출

`Document`에 대한 접근이 필요하므로, `LspService`가 `Document&` 참조를 갖거나
콜백 호출 시 Document를 외부(InputDispatcher)에서 넘겨주는 방식 선택 필요.

**권장: 콜백에서 Document 직접 수정** (InputDispatcher가 doc 보유)
```cpp
// InputDispatcher::AfterSpaceF 케이스
lsp_.formatting(uri_, [&doc](const nlohmann::json& edits) {
    auto text_edits = LspService::parse_text_edits(edits);
    // 역순 정렬
    std::sort(text_edits.rbegin(), text_edits.rend(), by_position);
    for (const auto& e : text_edits)
        doc.buffer().apply_text_edit(e.start_line, e.start_col,
                                     e.end_line,   e.end_col, e.new_text);
    doc.cursor().clamp();  // 편집 후 커서 범위 초과 방지
});
```

### 3. 상태 피드백 (선택)

포맷 적용 후 상태바에 일시적으로 "Formatted" 메시지를 표시하면 UX가 개선됩니다.
현재 상태바는 mode만 표시하므로, 이는 별도 작업.

## 영향 파일

| 파일 | 변경 |
|------|------|
| `src/core/entities/Buffer.hpp` | `apply_text_edit()` 추가 |
| `src/core/entities/Buffer.cpp` | `apply_text_edit()` 구현 |
| `src/core/usecases/LspService.hpp` | `parse_text_edits()` static helper 추가 |
| `src/core/usecases/LspService.cpp` | `parse_text_edits()` 구현 |
| `src/core/usecases/InputDispatcher.hpp` | AfterSpaceF 콜백에서 apply 호출 |
| `tests/unit/test_Buffer.cpp` | `apply_text_edit()` 유닛 테스트 |

## 테스트 시나리오

- 단일 TextEdit: 범위 내 텍스트가 newText로 교체됨
- 복수 TextEdit: 역순 적용 후 전체 텍스트 정합성 유지
- newText가 빈 문자열: 범위 삭제
- newText에 줄바꿈 포함: 멀티라인 삽입
- 편집 후 커서가 유효 범위 내에 남아 있음
- 실제 clangd 연동: `<Space>f` → 인덴트/줄바꿈 정규화 확인

## 전제 조건

없음 (독립 구현 가능)
