# ValveWise frontend rules

- Work only in `frontend/` and `server/`; never edit backend, src, include, data, tests, bench, tools, or root files.
- Keep all backend JSON access in `frontend/js/adapter.js`.
- Treat backend fields as an extensible contract: ignore unknown fields and tolerate missing or null optional fields.
- Display only values received from the engine; never hardcode simulation numbers.
- Use vanilla HTML/CSS/ES modules with no build step. Prefer small, testable modules.
- Keep fixtures generated from the backend CLI, or clearly record a fallback in NEEDS.md.
- Run adapter tests with `node --test frontend/tests/` before each milestone commit.
- Preserve user changes and make surgical patches.
- Branch is `codex/frontend`; do not touch main or other branches.
- Put ignore rules in `frontend/.gitignore` and `server/.gitignore`.
