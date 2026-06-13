# LSP Feature Backlog

미구현 LSP 피처 체크리스트. 각 항목은 독립된 설계 문서로 연결됩니다.

## 구현 우선순위 체크리스트

### 1단계 — 결과를 무시하고 있는 피처 (요청은 가지만 적용 안 됨)

- [x] **formatting** — [`formatting.md`](formatting.md)
  클라이언트가 `<Space>f`로 요청을 보내지만 clangd의 `TextEdit[]` 응답이 Buffer에 적용되지 않음.
  Buffer에 `apply_text_edits()` 추가 및 응답 처리 필요.

- [ ] **codeAction** — [`code-action.md`](code-action.md)
  `<Space>cA`로 요청을 보내지만 응답(액션 목록)을 표시하거나 실행하는 UI 없음.
  선택 UI + 실행 로직 필요.

- [ ] **rename** — [`rename.md`](rename.md)
  `<Space>rn` 키 시퀀스가 명시적 no-op. 새 이름 입력 프롬프트 UI와
  `WorkspaceEdit` 응답을 Buffer에 적용하는 로직 모두 없음.

### 2단계 — 트리거가 없는 피처 (코드는 있지만 호출 경로 없음)

- [ ] **completion** — [`completion.md`](completion.md)
  `LspService::completion()`, `render_completion()`, 결과 저장까지 모두 구현돼 있으나
  키 바인딩도 자동 트리거도 없어 실질적으로 사용 불가.

- [ ] **signatureHelp** — [`signature-help.md`](signature-help.md)
  `LspService::signature_help()`, `render_signature()` 구현됨.
  `(` 입력 시 자동 트리거 또는 명시적 키 바인딩 필요.

---

## 전제 조건 — 공유 인프라

위 피처들 중 복수가 의존하는 공통 구현이 필요합니다.

- [ ] **`Buffer::apply_text_edits()`** — formatting, rename 모두 필요
  LSP `TextEdit[]` (range + newText) 를 Buffer에 역순으로 적용하는 메서드.

- [ ] **Prompt 모드** — rename, (codeAction 선택) 에 필요
  한 줄 텍스트 입력을 받는 모달 UI. 새 EditorMode 또는 InputDispatcher 오버레이 상태로 구현.

---

## 범위 밖 (현재 프로토타입)

- `textDocument/rangeFormatting` — range 선택 UI 없음
- `workspace/symbol` 쿼리 입력 — Prompt 모드 없이 구현 불가 (현재 빈 쿼리 전송)
