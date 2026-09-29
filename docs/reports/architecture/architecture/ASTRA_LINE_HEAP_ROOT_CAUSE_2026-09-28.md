# Astra: persistent LINE contiguous-heap limit

2026-09-28. Architecture and memory forensics only. Next owner: **Sol Lite**.

## 1. Executive conclusion

**Exact allocation owner: NOT IDENTIFIED.** The best-supported model is a stable arrangement of live allocations within multiple capability-specific internal heaps, established during boot and/or the first quota TLS operations. The 34,804-byte value is the largest eligible free span at the sampled times, not proof of an ESP32 hardware maximum, a particular leaked object, or a 24 KB stack defect.

Two concrete findings improve the next investigation:

1. The recorded metrics use different capability filters. In the framework actually named by the existing linker map, `ESP.getFreeHeap()` and `ESP.getMinFreeHeap()` use **MALLOC_CAP_INTERNAL**; `LineNotifications::heapAdmitted()` and `systemDiagnostic()` obtain largest block with **MALLOC_CAP_8BIT**. Internal-only instruction RAM can contribute to the first value but cannot hold byte-addressed TLS buffers. If external RAM existed, 8BIT could conversely include memory outside INTERNAL. The 109–112 KB total therefore cannot be used as the denominator of a TLS fragmentation ratio.
2. The existing ELF has substantial static DRAM use and a persistent dynamic LINE stack, but both coexist with historical successful TLS. The 40 KB guard was explicitly introduced as a **planning/admission estimate**, not a measured minimum. This does not authorize lowering it.

Recommended next experiment: one diagnostic-only image that captures matching capability summaries at a small set of boot/request boundaries, plus a bounded **per-region allocator summary** at stable idle. First determine which region owns the 34,804-byte span and where the byte-capable capacity went. Do not start with threshold changes, task-stack reduction or broad network reordering.

## 2. Proven facts, unknowns and provenance

Repository `C:/ESP/esp32_035_lock_touch_test`, branch `master`, HEAD `d1e5ce38058790a973ce90c1addf33ef206ce4cd`. Existing `.pio/build/esp32_035/firmware.bin` is 1,238,752 bytes and its SHA-256 was re-read as `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`. Existing ELF and map were inspected, never rebuilt. Firmware-to-board verification is the preceding C5/C6 deployment evidence; this analysis did not contact the board or reconfirm its current running state.

Read the four primary reports: `ASTRA_SYSTEM_UNDERSTANDING_2026-09-28.md`, `SOL_POST_ASTRA_REVIEW_2026-09-28.md`, `SOL_C5_C6_DEPLOYMENT_ACCEPTANCE_2026-09-28.md`, and `SOL_LINE_HEAP_DELIVERY_FIX_2026-09-28.md`; also the LINE implementation/provisioning/web-delivery/friend-page and canonical-recurrence reports, relevant Git diffs, and saved heap JSON. Historical claims are explicitly distinguished from current source and artifact facts.

Tracked production source/lib/config has no diff from HEAD. **Qualification to the previous cleanup report:** this checkout also contains untracked `src/events/HeapTrace.cpp` and `HeapTrace.h`, apparently left by the interrupted worker. They have no current call sites and no HeapTrace symbols were found in the existing ELF/map. They are not evidence of deployed instrumentation. Because PlatformIO normally discovers source files under `src`, they can contaminate a future build even without a call site. Sol must resolve their intended disposition before the next build. Astra did not edit/delete them. All unrelated tracked/untracked WIP is preserved.

Saved matrix facts:

| Stage | Internal free | Internal minimum | Largest 8BIT block | heapBlocked |
|---|---:|---:|---:|---:|
| AP-on idle, seven samples | 109,336 | 55,268 | 34,804 | 16,818 → 17,002 |
| Immediately after normal LAN health request and AP-off | 111,888 | 55,268 | 34,804 | 17,138 |
| AP-off idle after inert page GETs | up to 112,144 | 55,268 | 34,804 | 17,238 → 17,421 |

The initial seven samples span about 184 seconds; +184 is about **60/minute**, not 31/minute. The later +183 over about 184 seconds corroborates it. The worker checks admission before testing whether any work is due; hence these are repeated denied checks, not thousands of attempted messages. Queue, event generation and push counters remain zero; TLS connections/responses remain 2/2. **Two quota TLS exchanges had already occurred in this boot.** No push in the observation window does not imply no prior TLS allocations.

Unknown: live region addresses/free spans, first point of collapse, allocated-block neighbors, current 8BIT total/minimum, PSRAM runtime registration, stack watermarks, TLS peak allocations, full authenticated browser allocation pattern, and any causal connection to a particular subsystem.

## 3. Actual ESP32 memory topology and framework

`platformio.ini` uses `espressif32@6.12.0`, `esp32dev`, Arduino. Crucially, the map records package path `C:/Users/fufu/.platformio/packages/framework-arduinoespressif32@3.20017.241212+sha.dcc1105b/`, not merely the unsuffixed package directory. The mapped framework is Arduino 2.0.17 with local `esp_idf_version.h` reporting **ESP-IDF 4.4.7**. The two installed packages' `dio_qspi/include/sdkconfig.h` hashes matched, but mapped paths should still be the authority. Toolchain ELF/map inspection used the installed Xtensa tools and read-only pyelftools decoding.

The ELF's `soc_memory_regions` and `soc_memory_types` were decoded according to local `heap_memory_layout.h` (16-byte region records, 20-byte type records). These are SoC candidate descriptors, **not a dump of the currently registered heaps**. Static/reserved ranges are removed and compatible adjacent ranges may be combined during heap initialization; aliases must not be counted twice.

| Candidate address ranges | Type/capability implications |
|---|---|
| `0x3ffae000–0x3ffe0000` | DRAM descriptors; byte-accessible, internal, DMA-capable capabilities; ROM/BT/static reservations reduce usable space |
| `0x3ffe0000–0x40000000` | D/IRAM descriptors; internal byte/DMA data view, with corresponding instruction aliases at `0x400a0000–0x400c0000`; startup-stack lifecycle matters |
| `0x40070000–0x400a0000` | IRAM candidate descriptors; INTERNAL, EXEC and 32BIT, **not 8BIT**; instruction/cache/static exclusions apply |
| `0x3f800000–0x3fc00000` | Candidate external SPIRAM window, not proof of an installed chip or registered heap |

The decoded DRAM capability union includes 8BIT/32BIT/DMA/INTERNAL/DEFAULT. IRAM type union `0x803` includes EXEC/32BIT/INTERNAL, excludes 8BIT. The SDK uses a capability-aware multi-heap allocator and TLSF, with `CONFIG_HEAP_POISONING_LIGHT=1`. Allocator headers, canaries, alignment, unavailable address gaps and region boundaries reduce allocatable payload. Coalescing cannot combine separated physical/registered heaps or span a live allocation.

Bluetooth support is compiled into the SDK, but that does not establish running BT tasks. `initArduino()` calls `esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)` when `btInUse()` is false; the existing ELF contains the weak default false implementation and no project BT use was found. Thus source predicts unused controller memory release before application setup. Do not recommend a duplicate release as the fix; actual region registration should be measured.

PSRAM: the prebuilt SDK supports it and `initArduino()` calls `psramInit()`, which only adds external RAM after successful chip initialization/test. The project supplies no proof of installed PSRAM, no explicit PSRAM application allocation and no populated external BSS section. `esp32dev` is a generic target, not hardware proof. Runtime `psramFound()` and SPIRAM total would settle this without reading data. Do not enable/rely on PSRAM speculatively. TLS is configured for internal allocation regardless.

## 4. Existing ELF: static/global RAM owners

The map and section table give `.dram0.data=25,164`, `.dram0.bss=87,448`, totaling **112,612 bytes** static DRAM. `.data` begins `0x3ffbdb60`; BSS begins `0x3ffc3db0`; `_bss_end`/`_heap_start=0x3ffd9348`. This leaves only 27,832 address bytes before `0x3ffe0000`, before other runtime boundaries/overhead; it is not the total runtime heap. Other eligible regions account for the rest.

IRAM vectors are 1,027 bytes, IRAM text 81,231 bytes, `_iram_end=0x40094154`. The address interval to `0x400a0000` is 48,812 bytes before exclusions/allocator overhead. This illustrates how tens of KB of INTERNAL memory may be unavailable for byte-oriented allocations; it is **not a measured 48,812-byte live free heap**. Flash rodata is 297,420 bytes and flash text 827,335 bytes. External RAM BSS/noinit sections are zero.

Largest material RAM symbols, measured by `nm -S`:

| Owner | Bytes | Lifetime/class |
|---|---:|---|
| AtomicFileStore write scratch | 8,192 | static BSS |
| AtomicFileStore setup-repair scratch | 8,192 | static BSS even on configured boot |
| AuthStore scratch | 8,192 | static BSS |
| legacy UserStore payload | 8,192 | static BSS |
| legacy DeviceStore payload | 8,192 | static BSS |
| LINE queue | 4,224 | static BSS, 16 slots |
| legacy loaded-device rows | 3,840 | static BSS |
| Wi-Fi `g_cnxMgr` | 3,800 | static BSS |
| AuthStore entries | 3,648 | static BSS |
| application identities array | 3,328 | static BSS |
| legacy loaded-user rows | 3,328 | static BSS |
| LINE config cache | 684 | static BSS, secret-bearing; contents not read |

The five 8 KB buffers total **40,960 bytes**, reside simultaneously for the entire process, and are not five live malloc blocks. They shift the DRAM heap start; they do not themselves create free-list holes. Their storage semantics cannot safely be merged merely because their sizes match. No storage redesign is recommended here.

Web assets measured in flash-mapped rodata: Management 53,165 bytes, Setup 20,666, Enrollment 10,091, Access 4,110. The lower-case `d` in nm does not mean DRAM; section/address determines placement. `send_P` serves these assets without building a whole-page RAM String. A larger HTML source is not automatically a larger persistent heap allocation.

## 5. FreeRTOS task and stack ownership

Local `portmacro.h` makes both task-control-block and dynamic-stack allocations `MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT`. Local task headers explicitly define stack depth and stack high-water mark in **bytes**, unlike upstream word-based assumptions. Dynamic task stack allocations are contiguous, persistent until task deletion, and compete with TLS-compatible heaps. Stack locals occupy this already-reserved block; do not add them a second time to heap consumption.

| Task | Configured stack / placement | Evidence and qualification |
|---|---|---|
| `line-worker` | 24,576 bytes; priority 1; core 0 | explicit `xTaskCreatePinnedToCore`; dynamic; never deleted in normal runtime |
| Arduino `loopTask` | 8,192 bytes; priority 1; configured Arduino core | `main.cpp`/sdkconfig; local HTTP, touch, auth, diagnostics execute here |
| `arduino_events` | 4,096 bytes; `ESP_TASKD_EVENT_PRIO-1`; configured event core | WiFiGeneric dynamic task creation |
| ESP default event loop | 2,048 bytes configured | system event configuration; separate from Arduino event dispatcher |
| `esp_timer` | 4,096 bytes configured | owns callback execution including timed relock; separate from loopTask |
| lwIP TCP/IP | 2,560 bytes; priority 18; core 0 | sdkconfig |
| mDNS | 4,096 bytes; priority 1; core 0 | sdkconfig, starts with responder |
| FreeRTOS timer daemon | 2,048 depth configured; priority 1 | distinct from esp_timer; do not conflate |
| IDLE0 / IDLE1 | 1,024 configured base each | actual allocation may include port overhead; dynamic/static startup details need runtime task inventory |
| Wi-Fi driver task(s) | core 0 configured | proprietary/prebuilt driver; exact live stack size not established by available app source |

There is no application-created HTTP task; synchronous WebServer runs on loopTask. ISR stack configuration is 2,096 bytes; this is not another normal application task. No live task watermark was captured. Compiled BT task settings are not evidence that BT tasks run.

The LINE stack can split a large eligible span, but it is **not proved to bound the 34,804-byte hole**. Reducing it by X bytes could improve largest block by zero if the released space is elsewhere; could grow an adjacent span by about X; or permit coalescing with another free span depending on allocation placement. Rebuilding with a smaller stack also changes subsequent placement. Its original 12,288-byte stack produced a real cold-handshake canary failure; halving it is not a defensible experiment without stack measurements and safety review.

## 6. LINE lifetime allocation audit

Boot initializes two mutexes and a wake semaphore, reads configuration into a static cache and creates the dynamic worker. The queue, counters, retry fields inside queue events, config cache and compiled root CA are not individually allocated on every idle loop. The root CA is constant data; `setCACert` uses a pointer to it, while TLS parsing creates temporary certificate structures.

Worker `Config` copies, current `Event`, text[1025], body[2048], request head[1200], line[256], response[2049], parser locals and nested mbedTLS call frames live on the worker stack. Some scopes overlap; compiler layout and TLS depth determine the actual watermark. Secret buffers are not permissible diagnostic output.

Each `lineRequest()` constructs a local WiFiClientSecure, whose constructor dynamically allocates `sslclient_context`. DNS/lwIP/TLS then allocate transient state. `stop()` tears down TLS contexts; the destructor calls stop and deletes the context. `TlsMeasurement` is declared before the client, so at scope exit its measurement occurs after the client's destructor. No global cached secure client is present. This argues against an intentionally retained entire TLS session, but does not prove allocator recovery, rule out a library leak or concurrent long-lived allocation during TLS.

Quota GETs use the same TLS path. The first two connections/responses in the failing boot mean TLS has already run, even though no push occurred. After largest drops below the guard, worker admission fails before `maybeQuota`/`takeEvent`; the loop retains its task stack/cache/semaphores and records another denial approximately every second. This explains the counter slope independently of the underlying memory cause.

## 7. HTTP and framework lifetime allocations

Application lifetime: WebServerManager, route handlers/lambdas and route URI objects are installed at startup and remain resident. Header-key objects remain to implement parsing. Management maintenance nonce and related state are bounded object members. A static Wi-Fi scan SSID String can retain capacity if scans are exercised; scan results are explicitly deleted. No scan was performed in the recent matrix.

Application temporaries: response JSON Strings, enrollment QR SVG reserving 16,000 bytes, 512-byte QR work buffer and identity row/payload allocations have request-scoped lifetimes. The simple unauthenticated page GETs did not exercise authenticated summaries, enrollment SVG generation or scan paths, so those paths remain unmeasured.

Framework retention: current vendored `WebServer::handleClient()` deletes the current argument array and null-assigns URI, host, response headers and captured-header values when retiring the request. The mapped Arduino `String::operator=(const char*)` calls `invalidate()` for nullptr, which frees non-SSO backing memory. This is real capacity release, unlike assigning an empty string. Upload/raw objects and current WiFiClient are reset. `_postArgs` and URI-handler `pathArgs` have separate parser/handler lifetimes and merit inspection only if those routes are active; existing cleanup is not proof that every framework object is transient.

Persistent handler allocations, TCP server/listener state, lwIP pbuf/PCB lifetimes and packet queues are not released by deleting Strings. A served request can overlap the LINE worker or leave socket-level transient state. No specific retained URI/header allocation explains the pre-existing plateau from the current evidence. Reapplying the completed-request cleanup would repeat a fix already present.

## 8. Wi-Fi, lwIP and mDNS ownership

Wi-Fi initialization creates AP and STA netifs, driver state, event dispatch, buffers and networking tasks. **Do not budget solely from sdkconfig:** it specifies eight static RX and eight static TX buffers, but Arduino `WiFiGeneric::_wifiUseStaticBuffers` defaults false. `wifiLowLevelInit()` overrides this to four static RX, 32 dynamic RX, zero static TX, 32 dynamic TX, cache TX four and dynamic TX mode. No project override was found. Exact live driver allocations still require region evidence; closed driver internals are not reconstructible from these counts alone.

AP shutdown recovered about 2.5 KB free but not the largest span. It does not deinitialize the still-active STA driver, all netifs, TCP/IP or mDNS. This disproves only the hypothesis that releasing current AP-specific allocations alone would restore admission in that run. Boot-time Wi-Fi allocation interleaving remains plausible.

mDNS owns a task, service/netif state and transient packet allocations. Current CanonicalOrigin leaves a healthy responder running and does not repeatedly update TXT records on address changes; that earlier mutation was removed in 979fc74. Persistent mDNS objects can separate free spans but no present allocation trace implicates them. No responder restart or network redesign is justified.

## 9. SD, storage, display and QR ownership

SD begins on a separate VSPI object, mounts FAT/VFS and retains card/mount/SPI bookkeeping. Arduino `sd_diskio.cpp` explicitly mallocs a card object; VFS/FAT structures and open-file objects have their own lifetimes. Files and directory handles in normal checks close; mount state remains resident. IdentityStore uses RAII `unique_ptr` 8,192-byte payloads and row arrays during loading/health/migration; these are temporary heap allocations, unlike the five static scratch arrays. Concurrent network activity can allocate between temporary storage blocks, but this is unmeasured.

The production display uses TFT_eSPI direct drawing; no project sprite/framebuffer creation was found. A hypothetical full 320×480×2 framebuffer would be 307,200 bytes, but it is not present in this implementation. QR rendering uses a 700-byte local buffer and draws modules directly; touch calibration is five uint16_t values. Font/bitmap assets are largely flash-resident. Display/SPI locks and driver objects are real smaller owners, but there is no evidence of a hidden full-screen framebuffer causing the limit.

## 10. Total-free versus largest-contiguous metrics

Current `free`/`min` are INTERNAL aggregates. Current `largest` is the maximum allocatable block across all 8BIT heaps. These filters overlap but are not identical. Minimum-free is a lifetime/aggregate low-water measure, not a per-request TLS allocation measurement; subtracting it from current free does not yield TLS peak. `gTlsMinimum` samples the same global low-water mark at request exit.

For matching capabilities, total free is the sum of multiple free spans while largest is a maximum over them. Four heaps with ~25–35 KB free each can provide >100 KB aggregate and no 40 KB allocation; so can one fragmented heap plus other regions. Neither model can be selected from a sum and a maximum alone. Query INTERNAL|8BIT jointly for TLS-relevant internal byte memory. Also query INTERNAL, 8BIT, DMA|INTERNAL and SPIRAM separately to expose filter/topology differences, without summing overlapping results.

## 11. Exact 34,804-byte plateau

34,804 is `0x87f4`. Its recurrence suggests the maximum comes from an unchanged eligible free span or an allocator reporting boundary while smaller spans elsewhere churn. The observed increase in total free need not touch that span or create a larger one. Light heap poisoning and allocator metadata mean a payload value need not equal an obvious power-of-two physical region. A size alone cannot identify the owner or prove TLSF rounding; inspect the region/block record first.

This is not a universal 32 KB region limit: the value exceeds 32 KiB and historical same-project readings reached 55,284/57,332 with the 24 KB LINE stack. Candidate SoC descriptors are not independent 8 KB/32 KB runtime heaps by definition. A fixed hardware-only 34,804 limit is strongly disfavored. A boot-specific fixed span bounded by persistent allocations is credible, but its address is absent from all current diagnostics.

## 12. Memory-relevant Git timeline

| Checkpoint | Memory-relevant change and evidential limit |
|---|---|
| 064518a / line-notifications-v1 | Added static RAM queue/config, TLS request buffers, task originally 12,288 bytes, and estimated 90 KB/40 KB guard. Removed operational history machinery. |
| 3fc128d, 48c653e | USB/physical provisioning state and bounded frame paths; later retired. No proof these historical buffers survive current source. |
| 14645a9 | Worker stack 12,288→24,576 after measured canary failure. Same priority/core; later live quota TLS largest 55,284 and successful push. |
| f750bf8, 8df5c8e, c5106ac | Evidence/receipt checkpoints; c5106ac itself changes documentation. Last explicitly confirmed receipt report identifies firmware source 14645a9. |
| 2b4fcf2 | Protected fallback lease/state and related UI; affects Wi-Fi lifetime/initialization interactions, not proof of fragmentation. |
| c229d41 | mDNS lifecycle/address handling and modem sleep disabled; adds state and changes runtime allocation timing possibilities. |
| 870ec7f | Flash HTML/Add Friend/UI changes and removal of active TFT fallback UI. No LINE transport/stack addition. Saved friend-page report records the 34,804 problem already before this flash. |
| 979fc74 | Removes USB provisioning implementation, adds web maintenance handlers/member state and ~dozens of bytes of telemetry atomics; releases request String/argument capacities and removes TXT mutation. Net memory cannot be inferred from added lines. Subsequent saved boots include largest 55,284. |
| d1e5ce3 | C5 fresh-install read-only predicates and C6 local gate correction; no new task, large persistent array or storage schema. The setup reads can affect temporary boot allocation timing but no causal measurement links them to the plateau. |

`git diff c5106ac d1e5ce3 -- src/storage` is empty: the large scratch owners predate the last receipt-confirmed baseline. The worker remains 24,576 throughout that interval. Current static RAM 112,612 is below the 113,500 recorded at the earlier TLS deployment, though report-to-artifact comparisons are historical and cannot establish identical runtime placement. No regression commit is isolated. Do not blame 870ec7f by chronology.

## 13–14. Ranked hypotheses and tests

### H1 — Boot/first-TLS allocation interleaving leaves stable bounded free spans

**CONFIDENCE:** Medium; strongest explanation class, not an identified owner.

**EVIDENCE FOR:** Fixed largest value with recovering aggregate, persistent stacks/network/storage objects, two prior TLS exchanges, other boots with larger spans.

**EVIDENCE AGAINST:** No region/address/block-lifetime evidence; a capability/topology-only explanation may suffice.

**HOW TO CONFIRM:** Find the region and unchanged live neighbor blocks around its largest span; show the stage where its size falls and fails to recover.

**HOW TO FALSIFY:** Region summaries show little internal fragmentation and all eligible regions structurally too small before these operations, or the reported largest changes while only aggregation/filters caused the apparent plateau.

### H2 — Capability mismatch and separate heap regions exaggerate available TLS capacity

**CONFIDENCE:** High for metric mismatch; medium for its share of this incident.

**EVIDENCE FOR:** Mapped Esp.cpp and ELF capability table directly differ; static layout and IRAM remainder are substantial.

**EVIDENCE AGAINST:** Largest still genuinely fails the configured 8BIT guard; mismatch alone cannot explain why earlier boots exceeded it.

**HOW TO CONFIRM:** Compare simultaneous INTERNAL and INTERNAL|8BIT totals and per-region 8BIT summaries.

**HOW TO FALSIFY:** Equal relevant capability totals and one sufficiently large eligible pool with many holes would make fragmentation, rather than filter mismatch, the dominant explanation.

### H3 — Persistent LINE/task-stack placement partitions the useful DRAM pool

**CONFIDENCE:** Medium-low for LINE specifically; high that it consumes eligible contiguous heap.

**EVIDENCE FOR:** 24,576-byte dynamic internal byte-capable stack allocated late in setup after network services.

**EVIDENCE AGAINST:** Prior successful 55,284-byte largest block with identical stack size; task placement not known; prior smaller stack overflowed.

**HOW TO CONFIRM:** Region snapshots immediately before/after task creation locate the reduction, with stack address range mapped to bounding blocks and high-water evidence.

**HOW TO FALSIFY:** Plateau exists beforehand or the stack occupies a different region and largest falls only later. Do not delete/reduce the task to test this initially.

### H4 — Residual TLS/library allocation or concurrent allocation during first quota requests

**CONFIDENCE:** Medium-low.

**EVIDENCE FOR:** Two successful TLS/response counters precede blocked steady state; first requests are a concentrated allocation phase.

**EVIDENCE AGAINST:** RAII client teardown is explicit; earlier repeated TLS worked; no measured unreleased block.

**HOW TO CONFIRM:** Matching region/block summaries before client construction and after full destruction show persistent new allocation(s), then identify caller/lifetime in a bounded trace.

**HOW TO FALSIFY:** Plateau predates first TLS or allocation/free topology fully returns afterward.

### H5 — HTTP/mDNS/Wi-Fi/SD persistent owner or retained capacity

**CONFIDENCE:** Low for any named individual; plausible collectively.

**EVIDENCE FOR:** Real persistent handler/mount/netif/task allocations and asynchronous work.

**EVIDENCE AGAINST:** AP-off and inert page requests do not change plateau; request cleanup is effective for the targeted Strings; static storage scratch does not form heap holes; no repeated live mDNS TXT mutation.

**HOW TO CONFIRM:** Subsystem checkpoint and region-neighbor evidence identifies the owner, then authenticated-request measurement if necessary.

**HOW TO FALSIFY:** Collapse occurs elsewhere and those owners' heap regions/lifetimes do not bound the affected span.

### H6 — Intrinsic 34,804 hardware ceiling, display framebuffer, corruption or progressive leak

**CONFIDENCE:** Very low / unsupported.

**EVIDENCE FOR:** Only the repeated number supports a stable bound; no positive framebuffer/corruption/leak evidence.

**EVIDENCE AGAINST:** Prior larger blocks, no production framebuffer, stable rather than monotonically shrinking free heap, no observed panic.

**HOW TO CONFIRM:** A failing heap integrity check or actual allocation trace/region limit, not numerology.

**HOW TO FALSIFY:** Intact heap and identified ordinary live boundaries account for the plateau.

## 15. TLS guard origin and local requirement analysis

**Basis: ESTIMATED initially; historical policy retained, not measured minimum.** `docs/design/LINE_SECURITY_NOTIFICATIONS_ARCHITECTURE.md:286–288` calls 40–70 KB transient TLS heap a planning estimate and 90 KB free/40 KB largest an initial admission target to tune from measurements. Git 064518a introduced the constants; 979fc74 factored them into telemetry without changing values. No searched evidence establishes a successful TLS call beginning below 40,000 or a measured worst-case contiguous requirement plus reserve. Historical success at 55,284 is not a minimum test.

The mapped SDK defines `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1`, maximum TLS content length 16,384, peer certificate retention and hardware crypto options. Record input/output buffers, certificate chain parsing, handshake state and cryptographic temporaries create multiple allocations; 16 KB content length does not imply a single 40 KB allocation or a 40 KB total bound. The local client uses a specific CA, not an application-selected full CA bundle. Exact negotiated-path peaks are unknown. The free guard's INTERNAL filter is not a measurement of TLS-compatible total. None of this is permission to lower either guard.

## 16. Exact missing evidence

- Simultaneous matched capability totals/largest/minimum/allocated/free-block counts and region addresses.
- Address and neighboring live allocations of the largest eligible free span.
- First boot stage or quota-request boundary where the span becomes 34,804.
- Runtime stack high-water marks and safely derived stack-region attribution.
- TLS before/after full teardown region comparison; global minimum alone is insufficient.
- Runtime SPIRAM presence and heap integrity result.
- Authenticated Management/QR path under controlled load; inert HTML GET is not equivalent.

## 17. Minimum diagnostic for Sol/Luna

Select a **region-first, two-tier diagnostic**, retaining the existing guards and all authorization. Existing production serial commands cannot reveal these fields; it requires separately reviewed diagnostic source/build/deployment. Do not use the orphan HeapTrace files as an approved implementation.

Tier 1: add a fixed read-only command that obtains `heap_caps_get_info()` for INTERNAL, 8BIT, INTERNAL|8BIT and INTERNAL|DMA, plus total SPIRAM and `psramFound()`. For each record include timestamp, total/allocated/free/minimum, largest, allocated/free block counts. Use `heap_caps_print_heap_info(INTERNAL|8BIT)` once at stable idle: local API documentation explicitly says it prints a per-heap summary followed by totals. This distinguishes distributed regions from internal holes. Also collect one `heap_caps_check_integrity(INTERNAL|8BIT,false)` boolean and LINE/loop stack high-water marks through known handles. No task enumeration that allocates an unbounded buffer; no secret/data bytes.

Capture fixed numeric snapshots into a bounded static array before printing. Budget approximately 2–4 KB for 16–24 compact records, calculate exact sizeof in the diagnostic build and document the BSS-induced shift. Store a phase/sequence for worker events; main and worker overlap, so timestamps do not prove a subsystem exclusively owns every delta. Release snapshot lock before printing; never print while holding allocator/application locks. Per-region printing traverses allocator state and costs time: do it on explicit read-only request at idle, not inside a critical lock or relock callback.

Tier 2 only if region summaries and first collapse do not identify the owner: narrowly compare allocator **block metadata** for the affected region before/after the implicated operation. `heap_caps_dump()` is available but can be verbose; review its local output implementation and bounded scope before enabling, never dump payload memory or NVS. Prefer a reviewed metadata-only region walker if a public API is available in the actual SDK; do not cast undocumented `registered_heaps` internals blindly.

Full allocation tracing is **not immediately available** in this prebuilt framework: `CONFIG_HEAP_TRACING_OFF=1`, and the trace header rejects unsupported use. A source define alone does not retrofit trace hooks into precompiled IDF libraries. Rebuilding framework instrumentation is a separate, more intrusive option. If Sol later approves it, scope one implicated startup/request interval with 256 fixed records and call-depth 2; calculate actual `sizeof(heap_trace_record_t)` and report memory overhead, overflow and changed timing. Start immediately before the operation, stop after full client destruction/idle, record only address/size/caller metadata, decode against matching ELF offline. No indefinite allocation logging. An allocation-failure hook alone will not explain this incident because the admission guard prevents the attempted allocation.

## 18. Boot-stage bisection plan

Keep the original initialization order and persistent-state behavior. Do not move AdminPin earlier merely to match a conceptual list. Record only existing-path snapshots, with no forced error or extra auth operation:

1. Earliest safe setup point **after LockController begin/HIGH**, before other setup work; this already includes Arduino/IDF startup and loopTask allocation.
2. After ConfigStore and after display/touch initialization (existing calibration only).
3. After SD mount/storage validation; after identity migration/integrity/auth checks already performed by normal boot.
4. Before Wi-Fi initialization; immediately after configured AP creation.
5. After captive DNS, after mDNS, after WebServer route registration/server begin, after saved STA initiation.
6. After existing AdminPin and installation metadata initialization; immediately before LINE begin.
7. Within LINE begin: after semaphore/config setup, immediately before and after worker creation. Worker capture also at task entry so racing startup is visible.
8. First settled STA/time state; before first quota request, after first request's complete client destruction, and after second quota request destruction.
9. First denied admission only, plus stable idle after 30–60 seconds and one later idle sample; no per-second log flood.
10. Only if needed after reviewing the above: one Management/inert Access request bracket and idle recovery, with HTTP parser/response/release checkpoints limited to that request.

The highest-value points are before/after LINE task creation and first quota TLS teardown **combined with region summaries**, not dozens of total-free logs. No LINE Test or door event is necessary to investigate the already-occurring quota path. Capture the diagnostic image's own map/hash and compare to the baseline because even added BSS can change heap layout.

## 19. Expected diagnostic interpretation

| Observation | Interpretation / next bounded step |
|---|---|
| INTERNAL exceeds INTERNAL|8BIT substantially | Metric mismatch is material; use matching TLS-capable totals for analysis, preserve guards pending Sol decision |
| Many healthy regions, each largest ≈ its free size, all below threshold | Region topology/capacity dominates; not necessarily fragmentation within regions |
| One eligible region has much more total free than largest, many free blocks | Intra-region fragmentation; identify live separating blocks |
| Largest falls at LINE task creation and stays | Task placement is implicated; map stack address and watermark, do not reduce immediately |
| Largest falls only after initial quota TLS, allocated bytes remain higher | Persistent concurrent/library allocation or leak candidate; bounded metadata trace |
| Total allocations return but largest does not | Same total with changed placement/interleaving, not automatically a leak |
| Largest already limited before network | Focus startup/static/storage/display/loopTask region topology |
| Largest falls at Wi-Fi/mDNS initialization | Isolate that phase's persistent owners with region metadata; no speculative architecture switch |
| Integrity fails | Stop deployment acceptance; investigate corruption before memory tuning |
| Diagnostic build alone changes plateau | Observer/layout effect; compare maps and smaller instrumentation, do not claim a fix |

## 20. Explicit non-actions and handoff

Only this report was written. No firmware/source/library/config/test edits, cleanup of orphan files, builds, tests, board probes/reboots, flashes, heap guard/priority changes, LINE messages, physical unlocks, credential/PIN reads or mutations, NVS/SD operations, partition/eFuse changes, Git commits/tags or destructive Git operations. No Luna or other agent was invoked. Existing artifacts were inspected read-only; no production implementation was performed.

**Astra stops here. Sol Lite owns the next decision:** review the capability mismatch, orphan diagnostic files and this region-first plan; select the minimum instrumented experiment; delegate only after defining the evidence gate. The exact root cause remains unproven and LINE/physical acceptance remains incomplete.
