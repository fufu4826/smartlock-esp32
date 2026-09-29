# Chapter 5 source index: Conclusion, limitations, and future work

| Original filename | Archive-relative location | What it contains | Suggested subsection |
|---|---|---|---|
| `FINAL_SMARTLOCK_ACCEPTANCE_2026-09-28.md` | `07_PHASE_REPORTS_COMPLETE/docs/phase_reports/FINAL_SMARTLOCK_ACCEPTANCE_2026-09-28.md` | Final software, deployment, and user physical acceptance reconciliation. | Conclusion and achieved functionality |
| `SOL_C5_C6_DEPLOYMENT_ACCEPTANCE_2026-09-28.md` | `07_PHASE_REPORTS_COMPLETE/docs/phase_reports/SOL_C5_C6_DEPLOYMENT_ACCEPTANCE_2026-09-28.md` | Final deployment and focused regression summary. | Benefits and acceptance evidence |
| `ASTRA_LINE_HEAP_ROOT_CAUSE_2026-09-28.md` | `06_ARCHITECTURE_AND_SECURITY/docs/architecture/ASTRA_LINE_HEAP_ROOT_CAUSE_2026-09-28.md` | Exact allocation owner remains unidentified; saved observations include sub-40 KB contiguous block and one denial. | Limitations and known reliability risk |
| `SOL_PRODUCTION_HEAP_REPRODUCTION_2026-09-28.md` | `07_PHASE_REPORTS_COMPLETE/docs/phase_reports/SOL_PRODUCTION_HEAP_REPRODUCTION_2026-09-28.md` | Production-only memory observations and their timing/attribution limits. | Limitations and evidence quality |
| `SOL_LINE_HEAP_MINIMAL_BISECTION_2026-09-28.md` | `07_PHASE_REPORTS_COMPLETE/docs/phase_reports/SOL_LINE_HEAP_MINIMAL_BISECTION_2026-09-28.md` | Bounded heap investigation and unresolved causal question. | Future work |
| `ASTRA_SYSTEM_UNDERSTANDING_2026-09-28.md` | `06_ARCHITECTURE_AND_SECURITY/docs/architecture/ASTRA_SYSTEM_UNDERSTANDING_2026-09-28.md` | State-preservation, threat, safety, and architecture review observations. | Maintenance and lessons learned |
| `IMPORTANT_COMMITS.md`, `DEVELOPMENT_TIMELINE.md` | `09_GIT_AND_RELEASE_HISTORY/` | Git-supported development history. | Lessons learned and maintenance |

Do not state that the intermittent largest-block value below 40 KB was fixed. The accepted workflow passed, while the heap behavior remains a known intermittent reliability risk. Revisit only with a bounded evidence-based investigation and preserve credential/state protections.