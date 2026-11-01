# Zephyr CTF 追蹤資料擷取與 Eclipse Trace Compass 視覺化分析指南

本指南記錄在 Zephyr RTOS（STM32 + DAP-Link）環境下，透過 SEGGER RTT 後端擷取二進位追蹤串流（`channel0_0`），並使用 Eclipse Trace Compass 配合自訂 XML 繪製執行緒調度甘特圖（Control Flow）的完整流程。

---

## 一、尋找 SEGGER RTT 控制區塊位址

OpenOCD 需要知道 `_SEGGER_RTT` 控制區塊在 MCU SRAM 中的位置，以建立記憶體通道讀取資料。

### 方法 A：從編譯產出的 ELF 檔查詢（手動查詢）

在專案目錄下使用 GNU `nm` 工具查詢符號表：

```bash
# 使用 Zephyr SDK 內建的 nm 工具
$ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm build/zephyr/zephyr.elf | grep -w "_SEGGER_RTT"

```

**輸出範例：**

```text
2000167c B _SEGGER_RTT

```

* **`2000167c`**：即為 RTT 控制區塊在 SRAM 中的起始位址（`0x2000167c`）。
* **`B`**：代表該變數存放在 BSS 段（開機初始化前為全零）。

---

### 方法 B：OpenOCD 自動記憶體掃描（免手動查位址）

OpenOCD 的 `rtt setup` 支援指定一段記憶體範圍進行字串比對。若不想每次重新編譯都查詢位址，可直接指定從 STM32 SRAM 起始位址（`0x20000000`）掃描 128KB 範圍：

```text
rtt setup 0x20000000 0x20000 {SEGGER RTT}

```

OpenOCD 會自動在整塊 RAM 之中搜尋符合 `"SEGGER RTT"` Magic 字串的位置。

---

## 二、透過 OpenOCD 與 RTT 擷取 `channel0_0`

### 1. 啟動 OpenOCD RTT 伺服器

執行以下指令連線 DAP-Link（CMSIS-DAP），將 RTT Channel 1（CTF 資料通道）綁定至主機端 TCP Port 9090：

```bash
openocd -f interface/cmsis-dap.cfg \
        -f target/stm32f4x.cfg \
        -c "adapter speed 4000" \
        -c "init" \
        -c "sleep 500" \
        -c "rtt setup 0x2000167c 0x100 {SEGGER RTT}" \
        -c "rtt start" \
        -c "rtt server start 9090 1"

```

> **參數關鍵說明：**
> * **`sleep 500`**：MCU 剛被重置時 BSS 段尚未初始化，需等待 500ms 讓 Zephyr 核心完成開機並執行 `SEGGER_RTT_Init()`。
> * **`{SEGGER RTT}`**：Jim Tcl 語法中包含空格的字串必須使用大括號包覆。
> * **`rtt server start 9090 1`**：開立本機 TCP Port 9090，對應 RTT Up Buffer Channel 1。
> 
> 

**終端機成功辨識訊息：**

```text
Info : rtt: Control block found at 0x2000167c
Info : Listening on port 9090 for rtt connections

```

---

### 2. 擷取串流資料

開啟第二個終端機視窗，使用 `netcat`（`nc`）連線 Port 9090 並將二進位資料重導向寫入檔案：

```bash
# 建立 trace 存放目錄
mkdir -p ~/Project/stm/trace
cd ~/Project/stm/trace

# 開始錄製 (按 Ctrl + C 結束錄製)
nc localhost 9090 > channel0_0

```

錄製結束後，確認檔案大小大於 0 bytes：

```bash
ls -lh channel0_0

```

---

## 三、Trace Compass 專案建置與檔案匯入

### 1. 準備目錄結構

Trace Compass 解析 CTF（Common Trace Format）時，要求追蹤目錄內必須同時包含 **`metadata`** 與資料檔 **`channel0_0`**：

```text
trace/
├── metadata        # 來自 Zephyr 原始碼 (subsys/tracing/ctf/metadata)
└── channel0_0       # 透過 OpenOCD 擷取出的二進位資料

```

---

### 2. 建立 Tracing 專案

1. 開啟 Eclipse Trace Compass。
2. 點擊頂部選單 **`File`** $\rightarrow$ **`New`** $\rightarrow$ **`Project...`**。
3. 展開 **`Tracing`** 分類，選擇 **`Tracing Project`**，點選 **`Next`**。
4. 輸入專案名稱（例如 `Zephyr_STM32`），點擊 **`Finish`**。

---

### 3. 匯入 CTF 檔案

1. 在左側 **`Project Explorer`** 視窗中展開剛建立的專案。
2. 在 **`Traces`** 資料夾上按**滑鼠右鍵** $\rightarrow$ 點選 **`Import...`**。
3. 點選 **`Browse...`**，選取包含 `metadata` 與 `channel0_0` 的 **`trace` 資料夾**。
4. 勾選左側清單中的 `trace` 資料夾。
5. 在右側 **`Trace Type`** 下拉選單中展開：
* 選擇 **`Common Trace Format`** $\rightarrow$ **`Generic CTF Trace`**。


6. 點擊 **`Finish`** 完成匯入。

---

## 四、配置 Zephyr 專用 Control Flow XML 分析檔

Trace Compass 內建的 Control Flow 視圖專為 Linux 核心排程事件（`sched_switch`）設計。針對 Zephyr 的 `thread_switched_in` 與 `thread_switched_out` 事件，需透過 XML 資料驅動分析建立自訂狀態機。

### 1. 建立 `zephyr_control_flow.xml`

在電腦中建立文字檔 `zephyr_control_flow.xml`，內容如下：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<tmfxml xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:noNamespaceSchemaLocation="xmlDefinition.xsd">

    <!-- 1. 狀態機定義 (State Provider) -->
    <stateProvider id="org.zephyr.thread.state" version="3">
        <head>
            <label value="Zephyr Thread State" />
        </head>

        <!-- 執行緒切入：依名稱給予整數狀態碼 -->
        <eventHandler eventName="thread_switched_in">
            <!-- 預設 (應用層/unknown): 狀態值 3 (綠色) -->
            <stateChange>
                <stateAttribute type="constant" value="Threads" />
                <stateAttribute type="eventField" value="name" />
                <stateValue type="int" value="3" />
            </stateChange>

            <!-- 若為 idle: 狀態值 0 (灰色) -->
            <stateChange>
                <stateAttribute type="constant" value="Threads" />
                <stateAttribute type="eventField" value="name" />
                <stateValue type="int" value="0" />
                <condition>
                    <field name="name" />
                    <stateValue type="string" value="idle" />
                </condition>
            </stateChange>

            <!-- 若為 tracing_thread: 狀態值 1 (紫色) -->
            <stateChange>
                <stateAttribute type="constant" value="Threads" />
                <stateAttribute type="eventField" value="name" />
                <stateValue type="int" value="1" />
                <condition>
                    <field name="name" />
                    <stateValue type="string" value="tracing_thread" />
                </condition>
            </stateChange>

            <!-- 若為 usbworkq: 狀態值 2 (藍色) -->
            <stateChange>
                <stateAttribute type="constant" value="Threads" />
                <stateAttribute type="eventField" value="name" />
                <stateValue type="int" value="2" />
                <condition>
                    <field name="name" />
                    <stateValue type="string" value="usbworkq" />
                </condition>
            </stateChange>
        </eventHandler>

        <!-- 執行緒切出：狀態清空釋放 CPU -->
        <eventHandler eventName="thread_switched_out">
            <stateChange>
                <stateAttribute type="constant" value="Threads" />
                <stateAttribute type="eventField" value="name" />
                <stateValue type="null" />
            </stateChange>
        </eventHandler>
    </stateProvider>

    <!-- 2. 時間軸甘特圖視圖與色彩定義 -->
    <timeGraphView id="org.zephyr.thread.view">
        <head>
            <analysis id="org.zephyr.thread.state" />
            <label value="Zephyr Control Flow" />
        </head>

        <!-- 整數值映射色彩定義 (HEX 色碼) -->
        <definedValue name="Idle Task" value="0" color="#BDBDBD" />
        <definedValue name="Tracing Thread" value="1" color="#AB47BC" />
        <definedValue name="USB Work Queue" value="2" color="#2196F3" />
        <definedValue name="App Thread" value="3" color="#4CAF50" />

        <entry path="Threads/*">
            <display type="self" />
        </entry>
    </timeGraphView>
</tmfxml>

```

> **說明**：Trace Compass XML 的 `<definedValue>` 屬性要求 `value` 必須為**整數（Integer）**。透過條件判斷將字串轉換為狀態碼 `0`、`1`、`2`、`3`，才能正常觸發指定的色彩。

---

### 2. 匯入 XML 分析檔

1. 在 Trace Compass 左側 **`Project Explorer`** 中，於 **`Traces`** 資料夾點擊右鍵 $\rightarrow$ 選擇 **`Manage XML Analyses...`**。
2. 點擊 **`Import...`** 按鈕，選取剛建立的 `zephyr_control_flow.xml`。
3. 確認清單中的 `Zephyr Thread State` 處於**勾選啟用狀態**，點選 **`Apply and Close`**。

---

### 3. 清除快取並開啟甘特圖

1. **清除舊快取**：
* 在左側 Project Explorer 找到已匯入的 Trace 資料夾（如 `trace`）。
* 點選右鍵 $\rightarrow$ **`Delete Supplementary Files...`**。
* **全選所有項目**後點擊 **`OK`**（強制清空舊的狀態快取資料庫）。


2. **開啟檢視視圖**：
* 雙擊開啟 `trace`。
* 展開 Trace 底下的 **`Views`** 資料夾。
* 雙擊 **`Zephyr Control Flow`** 視圖。



編輯區將呈現多色彩的執行緒調度甘特圖，清楚顯示 `idle`、`tracing_thread`、`usbworkq` 與應用程式執行緒的微秒級切換時序。