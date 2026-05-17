# Handoff: Syntactic Highlighting Implementation

**Branch:** `feat/lsp-e2e-tests`
**Date:** 2026-05-17
**현재 테스트 수:** 255/255 pass

---

## 배경

clangd semantic tokens는 함수, 타입, 변수 등 의미론적 식별자만 커버하며,
`keyword`, `comment`, preprocessor directive, string/char literal, number, operator는
**절대 emit하지 않는다** (clangd 17 설계 의도).

이 gap을 채우기 위해 state machine 기반 syntactic highlighting layer를 추가하는 작업이다.
구현 계획 전문: `docs/plans/2026-05-17-001-feat-syntactic-highlighter-plan.md`
HTML 시각화 문서: `docs/syntactic-highlighter-plan.html`

---

## 구현 단위 (U1~U4) 및 현재 완료 상태

### U1 — SyntaxHighlighter 상태 머신 구현 ✅ 완료

**완료된 작업:**
- `src/drivers/SyntaxHighlighter.hpp` 생성 (378줄, header-only)
  - `highlight_file(std::string_view source)` → `std::unordered_map<std::size_t, SpanList>`
  - `SpanList` alias 정의 (FtxuiRenderer의 내부 alias와 동일한 shape)
  - 6-state machine: `Code`, `LineComment`, `BlockComment`, `StringLit`, `CharLit`, `Preprocessor`
  - FTXUI / LspService include 없음 (독립적으로 테스트 가능)
- `tests/unit/test_SyntaxHighlighter.cpp` 생성 (465줄, 55개 테스트)
  - 모든 토큰 카테고리 커버
  - 핵심 edge case: `"// not a comment"`, `/* "not a string" */`, unterminated literal, block comment multiline, longest-match operator 등

**확인:**
```
ctest -R SyntaxHighlighter → 55/55 pass
```

### U2 — token_color에 새 타입 추가 ✅ 완료

**완료된 작업:**
`src/drivers/FtxuiRenderer.cpp` `token_color()` 함수에 3개 branch 추가:
```cpp
if (type == "macro" || type == "namespace" || type == "preprocessor")
    return ftxui::Color::BlueLight;
if (type == "parameter" || type == "variable") return ftxui::Color::Default;  // White→Default 버그픽스 포함
if (type == "syn_operator") return ftxui::Color::Cyan;
if (type == "bracket") return ftxui::Color::White;
```

또한 `#include "drivers/SyntaxHighlighter.hpp"`와 `#include <algorithm>` 추가됨.

**주의:** 이 변경들은 아직 **커밋되지 않은 상태** (unstaged).

### U3 — render_buffer에 merge 로직 연결 ❌ 미완료

**남은 작업:**
`src/drivers/FtxuiRenderer.cpp`의 `render_buffer()` 함수를 수정해야 한다.

현재 `render_buffer()`는:
```cpp
auto sem_tokens = lsp_.semantic_tokens();
// LSP token_map만 colorize_line에 전달
```

수정 후:
1. `buf.to_string()` 또는 전체 소스를 구성해서 `highlight_file(full_source)` 호출
2. 두 map을 merge (LSP wins on overlap)
3. merged map을 `colorize_line`에 전달

**merge 알고리즘 (plan에서 발췌):**
```
for each line i:
  merged = regex_map[i]  (syntactic spans, sorted by col)
  for each lsp_span in lsp_map[i]:
    remove or trim merged spans that overlap lsp_span
    insert lsp_span
  re-sort merged by col
  pass to colorize_line
```

overlap 4가지 케이스:
- LSP가 syntactic을 완전 포함 → syntactic 제거
- LSP가 syntactic 왼쪽을 덮음 → syntactic을 오른쪽으로 trim
- LSP가 syntactic 오른쪽을 덮음 → syntactic을 왼쪽으로 trim
- LSP가 syntactic 내부에 완전 포함 → syntactic 두 개로 분리 (또는 단순히 제거)

`test_SyntaxHighlighter.cpp`에 merge 테스트도 추가해야 함 (plan U3 "Test scenarios" 참조).

**Buffer::to_string() 존재 여부 확인 필요:**
```bash
grep -n "to_string" src/core/entities/Buffer.hpp src/core/entities/Buffer.cpp
```
없으면 `render_buffer()` 내에서 lines를 직접 join해서 `"\n"` 구분으로 넘기면 됨.

### U4 — 정리 및 커밋 ❌ 미완료

**남은 작업:**
1. `src/core/usecases/LspService.cpp`에 남아있는 임시 디버그 코드 제거:
   - `#include <fstream>` (debug용으로 추가됐던 것)
   - legend dump `ofstream` 블록 (있다면)
2. `ctest --output-on-failure` 전체 통과 확인
3. 커밋: `[FEAT][HIGHLIGHT] Add syntactic highlighting layer (keywords, comments, preprocessor, operators)`
4. `git push origin feat/lsp-e2e-tests`

---

## 현재 git 상태

```
Changes not staged for commit:
  modified:   src/core/usecases/LspService.cpp   ← 이전 세션의 debug 코드 잔재
  modified:   src/drivers/FtxuiRenderer.cpp       ← U2 변경사항 + 이전 bugfix들

Untracked files:
  src/drivers/SyntaxHighlighter.hpp               ← U1 신규 파일
  tests/unit/test_SyntaxHighlighter.cpp           ← U1 신규 파일
  docs/plans/2026-05-17-001-feat-syntactic-highlighter-plan.md
  docs/syntactic-highlighter-plan.html
  docs/handoff-syntactic-highlighter.md           ← 이 파일
```

---

## 다음 PC에서 시작하는 방법

```bash
# 1. 리포 clone 후 브랜치 확인
git checkout feat/lsp-e2e-tests
git pull

# 주의: SyntaxHighlighter.hpp, test_SyntaxHighlighter.cpp, FtxuiRenderer.cpp 변경사항은
# 현재 이 PC에만 있고 아직 push되지 않았음.
# 이 파일들을 직접 이 PC에서 복사하거나, 아래 내용을 참고해 재작성해야 함.
```

만약 파일들이 이전 PC에서 push되지 않은 경우, 이 handoff 문서와 함께
plan 문서(`docs/plans/2026-05-17-001-feat-syntactic-highlighter-plan.md`)를 참고해
U3부터 작업하면 된다.

---

## 핵심 파일 위치

| 파일 | 상태 | 역할 |
|------|------|------|
| `src/drivers/SyntaxHighlighter.hpp` | 신규 (미push) | 상태 머신 구현 |
| `src/drivers/FtxuiRenderer.cpp` | 수정 (미push) | token_color 확장, include 추가 |
| `tests/unit/test_SyntaxHighlighter.cpp` | 신규 (미push) | 55개 unit test |
| `src/core/usecases/LspService.cpp` | 수정 (미push) | 임시 debug 코드 제거 필요 |
| `docs/plans/2026-05-17-001-feat-syntactic-highlighter-plan.md` | 신규 (미push) | 전체 구현 계획 |

---

## 주의사항

- `SyntaxHighlighter.hpp`는 header-only. CMakeLists.txt 수정 불필요.
  기존 `GLOB_RECURSE`가 `test_SyntaxHighlighter.cpp`를 자동으로 포함함.
- `colorize_line` 함수는 **변경하지 않는다** (R10).
- LSP semantic token의 type string `"operator"`(Yellow)와 syntactic의 `"syn_operator"`(Cyan)는 구분된 별개 type string임.
- merge 후 SpanList는 반드시 col 기준으로 sort되어야 `colorize_line`이 올바르게 동작함.
