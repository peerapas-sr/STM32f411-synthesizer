# AGENT_HANDOVER.md - Detailed Conversation History & Handover Guide

> **Created for**: Claude Code CLI / External Agents
> **Source Session**: Antigravity Assistant (Conversation ID: `e254b936-2c29-49a3-84b3-f1b8ce12efe6`)
> **Project**: STM32F411RE Synthesizer & Sequencer (`z:\Embedsystemtoyota\Project`)

---

## 1. Quick Start with Claude Code
To continue this project seamlessly in Claude Code:
1. Open PowerShell and navigate to the project directory:
   ```powershell
   cd z:\Embedsystemtoyota\Project
   ```
2. Run Claude Code:
   ```powershell
   claude
   ```
   *(Claude Code will automatically ingest `CLAUDE.md` upon launch)*
3. Paste the following prompt to resume immediately:
   ```text
   ฉันต้องการให้คุณสานต่อโปรเจกต์ตามที่สรุปไว้ใน CLAUDE.md และ AGENT_HANDOVER.md:
   ช่วยดำเนินการลดขนาดโค้ด (Refactor & Condense) ให้ได้ตามเป้าหมาย ~1,605 บรรทัด (ลดลง ~50%) จากปัจจุบัน 3,284 บรรทัด โดยเน้นลดขนาด Src/app_synth.c และ Src/bsp_oled.c ตาม Option A (คงหน้าจอ Virtual Piano บน OLED ไว้) และต้องผ่านการ Compile 0 errors/0 warnings รวมถึงผ่าน 22 Toyota MISRA-C rules และ Zero-Polling UART/ADC 100%
   ```

---

## 2. Chronological History of User Requests
Below is the complete transcript of all key instructions given by the user in this session:

1. **User**: วันนี้มาทำ DMA กับ I2C DMA

2. **User**: ใช้ DMA ดียังไง

3. **User**: DMA เอาไปใ่ส่กับอะไรได้อีก

4. **User**: งั้นเอา DMA ใส่เข้า ADC เลย

5. **User**: [STM32 Timer-Triggered ADC Sampling with DMA Data Transfer | PCBCool](https://pcbcool.com/technical-guides/stm32-timer-triggered-adc-sampling-with-dma-data-transfer/)
ไอแบบนี้ปะ

6. **User**: สไตล์ 1 จัดเลยเพื่อน

7. **User**: วางแผน clean code อย่างละเอียด

8. **User**: จัดเลย

9. **User**: มีปัญหา จอ OLED ใช้ไปสักพักมันจะค้าง

10. **User**: มีปัญหา จอ OLED ใช้ไปสักพักมันจะค้าง หรือบางทีจอดับไปเลย

11. **User**: จัดเลย

12. **User**: ขอโน้ต fur elise หน่อย

13. **User**: จัดทำ โครงสร้าง โปรเจกต์นี้สำหรับให้ AI ตัวอื่นอ่าน

14. **User**: เพิ่มให้ ตอนกด ปุ่ม มันมีเสียงค้างไว้เล็กน้อย

15. **User**: เสร็จแล้ว วางแผนคลีนโค้ดอีกรอบ

16. **User**: จัดเลย

17. **User**: สรุปใส่ใน structure อีกรอบ

18. **User**: เปลี่ยนให้โหมด playback เล่นเป็น loop จนกว่าจะออกจากโหมด

19. **User**: โปรเจกต์นี้มีโค้ดกี่บรรทัดแล้ว

20. **User**: ขอความคืบหน้าสำหรับรอบนี้ไปเขียนเป็น project progress

21. **User**: ไม่ได้เพิ่ม DMA หรือ

22. **User**: ขอ block diagram แบบง่ายๆ

23. **User**: รกไปเห้ย ทำใหม่

24. **User**: ละเอียดกว่านี้นิดนึง

25. **User**: จัดดีๆ ไอควาย

26. **User**: ขอ block diagram ไปวาดใส่ Project progress แบบง่ายๆ

27. **User**: จากไฟล์ progress สัปดาห์ ที่แล้ว ต้องเขียนอะไรเพิ่มบ้าง ในสัปดานี้

28. **User**: โปรเจกต์นี้มีโค้ดกี่บรรทัด

29. **User**: ถ้าไม่ทำตามมาตรฐานโตโยต้า มันจะเหลือกี่บรรทัด

30. **User**: โปรเจกต์นี้มี intterrupt ไหม

31. **User**: สรุปมาว่า โปรเจกต์นี้มี peripheral , interrupt อะไรบ้าง

32. **User**: Project นี้ มีการใช้ Polling ไหม

33. **User**: ไม่ได้ดิเค้าห้ามใช้ pollinh

34. **User**: ถ้าแก้ โค้ดที่ใส่ลงไปกี่บรรทัด ใช้ท่ายากไหม

35. **User**: เขียนโค้ดต่างกับของตัวอย่างนี้มากไหม

36. **User**: งั้นแก้มา

37. **User**: /teamwork-preview ทำการดูภาพรวม Project นี้ เพื่อลดความซับซ้อนและความยาว ท่ายากของโค้ดในโปรเจกต์นี้

38. **User**: ลุย

39. **User**: optionA ก่อนทำเช็คให้ชัวร์ว่าอัพโค้ดปัจจุบันขึ้น github แล้ว จากนั้นลงมือทำได้เลย

40. **User**: ทำไม refactor แล้ว มันโค้ดเพิ่มขึ้น

41. **User**: 2. ตารางผ่าโค้ดรายโมดูล (Module-by-Module Breakdown)
โมดูล / ไฟล์	บรรทัดปัจจุบัน	บรรทัดเป้าหมาย	บรรทัดที่ลดได้	% ที่ลด	"ท่ายาก" และความซับซ้อนเกินจำเป็นที่ตรวจพบ
Src/app_synth.c	674	320	-354	-52.5%	สูตรคำนวณ Pitch Bend พหุนามอันดับ 7 ที่ซับซ้อน, ระบบ Debounce ซ้ำซ้อน 4 ปุ่ม, การจัดการคอร์ด FSM ยืดยาว
Src/bsp_oled.c	694	240 / 410*	-454 / -284*	-65.4%*	ลูปวาดลิ่มเปียโนแบบทีละพิกเซลยาว 340 บรรทัด, การส่งคำสั่ง Keep-Alive ซ้ำซ้อน, ตัวนับ Error หลายชั้น (*ขึ้นกับ Option ที่เลือก)
Src/bsp_joystick.c	273	120	-153	-56.0%	State Machine คาลิเบรตหาจุดกึ่งกลางที่ซับซ้อนเกินจำเป็น, มีฟังก์ชันไม่ได้ใช้งานละเมิด MISRA Rule 8 (bsp_joystick_is_pressed)
Src/bsp_buzzer.c	260	120	-140	-53.8%	การสเกลความดัง Volume ทำซ้ำซ้อน 2 ต่อ (ตาราง LUT + สูตรคำนวณ), การเขียนรีจิสเตอร์ Timer ซ้ำซ้อน
Src/bsp_adc.c	231	140	-91	-39.4%	การทำ Software Moving Average 16 ตัวซ้อนทับ ทั้งๆ ที่ฮาร์ดแวร์ DMA Circular อ่านค่าตรงอยู่แล้ว
Src/bsp_uart.c	280	180	-100	-35.7%	การจัดระเบียบ Ring Buffer หลายชั้นเกินไป, การแตกเงื่อนไข Parse คำสั่งยาวเกินจำเป็น
Src/bsp_timer.c	215	120	-95	-44.2%	แยกตัวนับ microsecond กับ millisecond ซ้ำซ้อน, โครงสร้าง delay loop ยาวเกินไป
Src/bsp_gpio.c	179	110	-69	-38.5%	การเขียน Bitwise ตั้งค่าพิน 12 ตัวแยกทีละบรรทัดแบบ Manual แทนที่จะใช้ Loop/Table กะทัดรัด
Src/main.c	194	145	-49	-25.3%	การประกาศ extern ข้ามโมดูลกระจัดกระจาย, ลำดับการ Init ยาวซ้ำซ้อน
Headers (Inc/*.h)	210	110	-100	-47.6%	มาโครที่ไม่ได้ใช้, Typedef ซ้ำซ้อน, Prototype ฟังก์ชันที่ถูก Clean Code ออกไป
รวมทั้งโปรเจกต์	3,210	~1,605	-1,605	-50.0%	ลดขนาดโค้ดได้ครึ่งหนึ่ง โค้ดสะอาดตา อ่านง่ายขึ้นมาก
เอ้าแล้วไหนลดได้ 50 %

42. **User**: วิธีเอาประวัติแชทไปให้ claude code


---

## 3. Key Technical Decisions & Invariants
1. **Toolchain Paths (Windows STM32CubeIDE plugins)**:
   - GCC: `Z:\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin`
   - Make: `Z:\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin`
2. **Zero Polling Invariant**:
   - USART2: Handled by `USART2_IRQHandler` (RXNE buffer `g_c_rx_buffer`, TXE buffer `g_c_tx_buffer`). Never introduce polling `while(!(USART2->SR & ...))`.
   - ADC1: Triggered autonomously by TIM3 TRGO into DMA2 Stream 0 circular buffer. Never call `bsp_adc_read()` or poll `ADC_SR_EOC`.
3. **EXTI Invariant**:
   - PC2 Joystick SW triggers `EXTI2_IRQHandler` setting `g_b_joy_sw_exti_flag` (moved from PA10 / EXTI10).
4. **Option A UI Invariant**:
   - 1.30" SH1106 OLED displays Virtual Piano keyboard (8 white keys + 5 black keys).
   - Invert-fill for active keys accelerated using Page-Byte stride optimization (Pages 3..7, 70 byte writes).
5. **Pitch Bend Invariant**:
   - 21-point Q12 ratio table (`PITCH_RATIO_Q12`) for `[-250, +250]` cents bend with linear interpolation.

---

## 4. Why Lines Grew in Pass 1 & How to Fix in Pass 2
- **Pass 1 Issue**: Pass 1 replaced the algorithm (Taylor polynomial -> Q12 LUT table, pixel loop -> byte stride), but left all the verbose FSM wrappers, large comments, and added MISRA boilerplate (`else { /* ... */ }` on every `if`). This caused line count to increase by +74 lines to 3,284 lines.
- **Pass 2 Remedy**:
  1. `Src/app_synth.c`: Collapse the 8 individual sequencer start/stop/toggle functions into unified state transitions. (Note: the original plan to remove empty `else` blocks was WRONG for this course - the Toyota skill file requires an `else` after every `if`; they were restored.)
  2. `Src/bsp_oled.c`: Format `OLED_FONT5X7` compactly (pack 4 chars per line, saving ~50 lines). Streamline `oled_i2c_bus_recovery` and low-level I2C timeout loops.
  3. `Src/bsp_gpio.c`: Replace the 45-line `if-else` cascade in `bsp_gpio_read_keys()` with branchless bitwise reading (saves ~35 lines).
  4. `Src/bsp_joystick.c`: Streamline the switch debounce state machine.

---

## 5. Pass 2 Result (Claude Code)
- Total: **3,284 -> 2,331 lines (-29%)** after the Toyota rule fixes. See `CLAUDE.md` "Current Status" for the per-file table.
- Toyota rule fixes: `else` added after all 55 single `if`s (Rule 19), magic numbers replaced by `#define`s (Rule 5), unreachable loop in `main` and dead branch in `bsp_buzzer_set_tone` removed (Rule 7), and 4 tricks rewritten plainly (triangle LFO, LED mask shifts, `oled_wait` bool comparison, XOR debounce).
- Build 0 errors / 0 compiler warnings; `verify_misra.py` 0 violations; UART (RXNE/TXE ISR) and ADC (TIM3 TRGO + DMA2) still zero polling.
- Features kept: 8 notes + bank switch, pitch bend, vibrato, 150 ms sustain, record / loop playback, K1+K4 / K2+K3 combos, UART commands and telemetry, OLED Virtual Piano with page-byte stride.
- Behaviour changes: combo-skew note purge only on the K1+K4 combo; toggling playback while recording saves the recording first; `c` returns to live mode before clearing; `P:+`/`P:-` pixel dot removed; OLED shows a cleared screen until the first frame; startup chime notes are all 50 ms.
- API changes: `bsp_adc_get_raw()`, `bsp_uart_read_char()`; removed `bsp_joystick_init()` and public OLED drawing primitives.
- ~1,605 was not reached; it needs feature removal (user decision pending).
- Not yet verified on hardware.
