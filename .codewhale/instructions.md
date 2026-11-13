# Anvil IDE — session bootstrap

Read `PROJECT.md` at the repository root before doing any work. It is the
single source of truth for product concept, architecture, security invariants,
build/test commands, current release state, roadmap, and the owner-maintained
`Bugs` and `Improvements` backlog.

Rules:

1. Do not read `prompts/masterprompt_0.0.1.md` unless the owner asks;
   `PROJECT.md` summarizes it. Open only the source files relevant to the
   selected backlog items.
2. Confirm the target release and the `BUG-xxx` / `IMP-xxx` IDs before coding.
3. Never weaken the invariants in `PROJECT.md` §2. If a request conflicts with
   one, stop and name the conflict.
4. Run the verification commands in `PROJECT.md` §5 and update the statuses and
   changelog in `PROJECT.md` when the work is done.
5. Do not push to git, rewrite history, or start out-of-scope work.
