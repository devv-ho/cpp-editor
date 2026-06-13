# textDocument/signatureHelp

## 현황

| 항목 | 상태 |
|------|------|
| LSP 요청 전송 | ✅ `LspService::signature_help()` |
| 응답 파싱 | ✅ `set_signature()` — signature 문자열 저장 |
| UI 렌더링 | ✅ `render_signature()` — 노란색 텍스트로 표시 |
| 트리거 | ❌ 없음 |

## 문제

`render_signature()`까지 모두 구현돼 있지만 `lsp_.signature_help()`를 호출하는 경로가 없어서 `signature_text_`가 항상 비어 있습니다.

## 설계

### 트리거 방식

signatureHelp는 **자동 트리거가 자연스럽습니다** — 함수 호출의 `(` 입력 시 자동으로 발동하는 것이 VS Code, CLion 등 모든 에디터의 표준 UX.

```
Insert 모드에서 '(' 입력
  → EditorCommands::insert_char 실행 (Buffer에 삽입)
  → 이후 signature_help 요청 트리거
```

### 구현 방법

**방법 A — InputDispatcher에서 insert_char 후 트리거 (권장)**

```cpp
// InputDispatcher::dispatch_insert() 또는 insert_char 처리 경로
if (cmd == Command::insert_char && ch == '(') {
    // insert_char 정상 실행 후
    auto pos = doc.position();
    lsp_.signature_help(uri_, pos.line, pos.col, [&lsp=lsp_](std::string sig) {
        lsp.set_signature(std::move(sig));
    });
}
```

**방법 B — EditorCommands::insert_char에 후크 추가**
- 레이어 위반 (core entity가 LSP 의존): 부적합

**권장: 방법 A**

### ')' 입력 시 overlay 해제

함수 호출이 닫히면 signature overlay를 닫아야 합니다:
```cpp
if (cmd == Command::insert_char && ch == ')') {
    lsp_.set_signature("");  // overlay 해제
}
```

### 명시적 키 바인딩 (보조)

자동 트리거 외에 `Ctrl-K` 또는 별도 키로 Insert 모드에서 수동 트리거도 추가 가능.
현재 Normal 모드에서 `K`는 hover로 사용 중이므로 Insert 전용 키 필요.

### render_signature() 현황

이미 구현된 렌더링:
```
void: myFunc(int a, std::string b)
```
노란색(Yellow) 텍스트로 buffer 아래에 표시. 추가 변경 불필요.

선택적 개선: active parameter 강조 (`,` 개수로 현재 파라미터 추적).

## 영향 파일

| 파일 | 변경 |
|------|------|
| `src/core/usecases/InputDispatcher.hpp` | Insert 모드 `(` 입력 시 signature_help 트리거 추가 |
| `tests/unit/test_EditorCommands.cpp` | `(` 입력 시 signature_help 호출 확인 |

## 테스트 시나리오

- Insert 모드에서 `(` 입력 → signature_help 요청 발생
- 응답 수신 후 `render_signature()` overlay 표시
- `)` 입력 → overlay 해제
- 함수가 아닌 위치에서 `(` → clangd가 빈 응답 → overlay 없음
- Escape → Insert 모드 종료, overlay 해제

## 전제 조건

없음 (가장 단순한 구현, 독립적)
