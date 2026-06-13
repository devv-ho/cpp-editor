# textDocument/codeAction

## 현황

| 항목 | 상태 |
|------|------|
| LSP 요청 전송 | ✅ `LspService::code_action()` |
| 응답 파싱 | ✅ `parse_code_actions()` → `std::vector<LspCodeAction>` |
| 결과 저장 | ❌ 콜백 내 지역 변수로만 존재, 저장 안 됨 |
| 선택 UI | ❌ 없음 |
| 실행 로직 | ❌ 없음 |
| 키 바인딩 | ✅ `<Space>cA` (요청은 보내지만 결과 무시) |

## 문제

`<Space>cA` → `lsp_.code_action()` 까지는 실행되고, clangd가 `LspCodeAction` 목록을 콜백으로 전달하지만 콜백 내에서 결과가 버려집니다.

```cpp
// InputDispatcher 현재 코드 (AfterSpaceC)
lsp_.code_action(uri_, pos.line, pos.col, [](const std::vector<LspCodeAction>& actions) {
    // actions가 여기서 소멸됨 — 표시/실행 없음
});
```

## 설계

### LspCodeAction 구조

```cpp
struct LspCodeAction {
    std::string title;       // "Add #include <string>"
    std::string kind;        // "quickfix", "refactor", etc.
    nlohmann::json edit;     // WorkspaceEdit (선택적)
    nlohmann::json command;  // LSP Command (선택적)
};
```

clangd가 보내는 액션은 두 종류:
1. `edit` 필드 있음 → `WorkspaceEdit`를 직접 적용
2. `command` 필드 있음 → `workspace/executeCommand` 요청을 추가로 보내야 함

### 구현 흐름

```
<Space>cA
  └─ lsp_.code_action() → 응답 수신
       └─ set_code_actions(actions)  ← LspService에 저장
            └─ on_update_() 트리거
                 └─ FtxuiRenderer::render_code_actions() — 목록 표시
                      └─ 사용자가 번호키(1~9) 또는 Enter로 선택
                           └─ 선택된 액션 실행 (edit 적용 또는 executeCommand)
```

### 1. LspService에 code_actions 저장

```cpp
// LspService.hpp에 추가
void set_code_actions(std::vector<LspCodeAction> actions);
std::vector<LspCodeAction> code_actions() const;

// 내부 멤버
std::vector<LspCodeAction> code_actions_;
```

### 2. `render_code_actions()` — 선택 UI

기존 `render_completion()` 패턴과 유사한 목록 박스:
```
╭── Code Actions ───────────────────╮
│ 1  quickfix  Add #include <string> │
│ 2  refactor  Extract to function   │
│ 3  quickfix  Fix typo: 'retun'     │
╰───────────────────────────────────╯
```
- 최대 9개 표시 (숫자키 1~9로 선택)
- `Enter` → 첫 번째 항목 선택
- `Escape` → 취소, overlay 닫기

### 3. 액션 실행

**edit 타입 (WorkspaceEdit):**
```cpp
auto edits = LspService::parse_text_edits(action.edit["changes"][uri]);
// Buffer::apply_text_edits() 역순 적용 (formatting.md 참조)
```

**command 타입:**
```cpp
lsp_.execute_command(action.command);
// LspService에 workspace/executeCommand 요청 추가 필요
```

### 선택 입력 처리

Code Action 목록이 표시된 상태에서 숫자키를 누르면 해당 액션 실행.
이를 위한 새 InputDispatcher 상태: `AfterCodeActionList`.

```
AfterSpaceC → lsp_.code_action() 호출
AfterCodeActionList → '1'~'9': 해당 액션 실행 후 Normal
                    → Escape: 취소, Normal
```

## 영향 파일

| 파일 | 변경 |
|------|------|
| `src/core/usecases/LspService.hpp` | `set_code_actions()`, `code_actions()` 추가 |
| `src/core/usecases/LspService.cpp` | 위 메서드 구현 |
| `src/core/usecases/InputDispatcher.hpp` | `AfterCodeActionList` 상태 추가 |
| `src/core/interfaces/Command.hpp` | `select_action_1`~`select_action_9` 커맨드 (또는 숫자 파라미터) |
| `src/drivers/FtxuiRenderer.cpp` | `render_code_actions()` 추가 |
| `tests/unit/test_LspService.cpp` | `set_code_actions()` 테스트 |

## 테스트 시나리오

- `<Space>cA` → code_actions overlay 표시
- Escape → overlay 닫힘, Normal 복귀
- '1' 입력 → 첫 번째 액션 실행
- edit 타입 액션 → Buffer 수정 확인
- 액션 없는 경우(빈 목록) → overlay 표시 안 함

## 전제 조건

- `Buffer::apply_text_edits()` (formatting.md와 공유, edit 타입 액션에 필요)
- `workspace/executeCommand` LSP 요청 (command 타입 액션에 필요, 선택적)
