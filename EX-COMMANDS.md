# Ex command continuation

The following command pairs were implemented in separate local commit cycles.
The English and German manuals describe their syntax and WinVi behavior.

| Commands | Implementation | Local regression suites |
| --- | --- | --- |
| `source`, `shell` | `ExScript.c` | `ex-script-test.cmd`, `ex-script.ps1` |
| `map`, `unmap` | `ExMap.c`, shared Settings macro engine | `ex-map-test.cmd`, `ex-map.ps1` |
| `abbreviate`, `unabbreviate` | `ExMap.c`, shared Settings abbreviation engine | `ex-map-test.cmd`, `ex-abbreviation.ps1` |
| `preserve`, `recover` | `Recovery.c`, `ExRecovery.c`, staged page adoption | `recovery-test.cmd`, `ex-recovery-test.cmd`, `ex-recovery.ps1` |
| `ex`, `exit` | Existing `edit` and `xit` dispatch paths | `ex-alias.ps1` |

The suites live under `tests/` and are registered in `test.ps1`. Native suites
with an ASan configuration accept `asan` as their first argument. GUI suites
use `WINVI_TEST_EXE` to select the executable and `WINVI_TEST_CASES` to keep
dummy files inside a chosen test directory.

The release target remains Windows 7 SP1 with KB2533623 or a superseding
update, Windows 8/8.1, Windows 10 and Windows 11 x64. See
`WINDOWS-COMPATIBILITY.md`. These command cycles were tested locally in release
and ASan builds; Windows 7 VM checks remain a separate batch.

## Follow-up syntax work

- General command chaining with `|`, including quoting, escaping, error handling
  and undo boundaries.
- Semicolon-separated addresses with the required current-line updates while
  parsing subsequent addresses.

Terminal-only `open`/`visual` modes and Unix `suspend`/`stop` commands are outside
the Windows GUI command plan. This inventory records the agreed additions;
the manuals document the supported command syntax and implementation limits.
