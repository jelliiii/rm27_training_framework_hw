# 协作方式

- 用户以独立学习和编写为主；检查实际代码后给出指导，未要求实现时不代写业务代码。
- 每次代码检查后更新本文件的“当前指导”：保留未完成事项、删除已解决项，不追加冗长历史。
- 每次尽量只列 3～5 项，优先给短代码示例；区分已验证、待修改和仅建议。
- 尊重用户已有修改。没有硬件时，只能报告静态检查或实际编译结果，不能宣称通信已验证。

# 当前指导（2026-10-03，CAN）

已检查：双 CAN 引脚和 1 Mbit/s 静默回环已生成；未执行编译或硬件测试。

1. 先在 CubeMX 勾选 CAN1 RX0、CAN2 RX0 中断并重新生成。当前缺少对应 NVIC 配置和 IRQHandler。
2. 修复 `bsp/src/bsp_can.cpp`：第一次调用 `HAL_CAN_ConfigFilter()` 前就初始化分界字段；所有 HAL 配置/启动调用检查返回值。

```cpp
CAN_FilterTypeDef filter = {};
filter.SlaveStartFilterBank = 14;
filter.FilterBank = 0;  // CAN1 用 0，CAN2 用 14
// 补齐模式、位宽、FIFO、使能等字段后再调用 HAL。
```

3. 按职责拆分，以下为建议接口，尚未实现；改签名时同步头文件、实现及调用处。

```cpp
// bsp_can.cpp 内部使用
static HAL_StatusTypeDef CAN_ConfigFilter(CAN_HandleTypeDef* hcan);
static HAL_StatusTypeDef CAN_StartRx(CAN_HandleTypeDef* hcan);
// 对外接口
HAL_StatusTypeDef CAN_Init(void);  // 组织两路过滤器配置、启动及接收通知
HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef* hcan,
    uint32_t id, const uint8_t* data, uint16_t len);
// 回环调试函数只提交测试帧，接收由中断回调验证；不轮询抢读 FIFO。
```

发送先检查句柄、标准 ID <= 0x7FF、长度 <= 8；使用清零的本地 8 字节数组复制有效载荷，再交 HAL（本地 HAL 会读取全部 8 字节）。邮箱满返回 HAL_BUSY，提交成功不等于对端收到。

4. 将 BSP 加入 `board/CMakeLists.txt` 对应位置，并确认交叉工具链配置了 C++ 编译器：

```cmake
enable_language(C CXX ASM)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ../bsp/src/bsp_can.cpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ../bsp/include)
```

5. `main.c` 调用 `.cpp` 接口时，在 `bsp_can.hpp` 的对外函数声明外加 C 链接保护；HAL 接收回调保持 C 链接。初始化只执行一次，调试入口用编译开关隔离。

```cpp
#ifdef __cplusplus
extern "C" {
#endif
// CAN_Init、CAN_Transmit 及需要从 main.c 调用的调试函数声明
#ifdef __cplusplus
}
#endif
```

下一步：完成 1、2、4、5，补完发送函数及 RX0 回调后，再检查编译和回环验证逻辑。

---

# 记录 002｜2026-10-03｜C 链接、CMake 与完整 CAN 建议

## 最新协作约定（覆盖上方旧约定，旧记录保留）

- 本项目内只允许写入 `AGENTS.md`，不得直接修改源码、配置、生成文件或构建产物。
- 每次检查追加带日期、编号的记录，保留全部旧记录；在新记录中注明哪些旧问题已解决。
- 建议尽量清楚紧凑；用户要求完整代码时，提供完整示例及放置位置，不能只有接口声明。
- 文档中的代码只是建议，不代表已经应用；报告真实验证范围。

## 本次检查结论

静态检查，未执行构建或硬件验证。已解决：CAN1/CAN2 RX0 的 NVIC 与 IRQHandler 已生成，USART1_TX 已改为 PA9。

1. HAL 已接入 `board/cmake/stm32cubemx/CMakeLists.txt`：包含 `can.c`、`stm32f4xx_hal_can.c`、HAL/CMSIS 头文件目录、`USE_HAL_DRIVER`、`STM32F407xx`。`stm32f4xx_hal_conf.h` 已开启 `HAL_CAN_MODULE_ENABLED`。
2. `board/CMakeLists.txt` 把 `../bsp/src/bsp_can.cpp` 错拼为 `../bsp/src/basp_can.cpp`。
3. 根目录 `build/Debug/CMakeCache.txt` 的源目录仍是项目根目录，而实际生成工程在 `board/`；这个缓存不能证明 VS Code 当前选择，但说明之前配置过错误入口。
4. `CAN_Transmit()` 仍为 `void`、数据参数仍为非 const，而 `CAN_DebugSend()` 传 const 数组并返回其结果，两处类型不匹配。
5. 过滤器第一次配置前未初始化 `SlaveStartFilterBank`；接收回调为空，未取走 FIFO 中的帧，开中断后可能反复进入中断。
6. SPI/I2C 尚未启用，相关 HAL 模块宏也未开启。这些类型暂时不可用不能靠随意补 include 修复，需先配置对应外设并生成。

## A. C 链接保护是什么

`main.c` 按 C 编译，`bsp_can.cpp` 按 C++ 编译。C++ 为支持函数重载通常会修饰链接符号名；C 调用方与 C++ 定义方约定不一致时可能出现 undefined reference。

```cpp
#ifdef __cplusplus      // 只有 C++ 编译器进入此段
extern "C" {            // 让下面的接口使用 C 语言链接约定
#endif
HAL_StatusTypeDef CAN_Init(void);
#ifdef __cplusplus
}
#endif
```

这不是把 C++ 翻译成 C，也不是中断保护。`#ifndef BSP_CAN_H` 是另一种保护：避免头文件被重复展开。对外接口需使用 C 能理解的类型，不能把 class/模板放进 C 接口。实现 .cpp 包含此头文件后，同名匹配定义继承该声明的链接属性。

## B. 建议替换 bsp/include/bsp_can.hpp 的完整内容

保留现有文件名；内容是 C 也能解析的接口。后面的实现与此声明必须一起采用。

```cpp
#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef CAN_Init(void);
HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef* hcan,
    uint32_t id, const uint8_t* data, uint16_t len);

#ifdef CAN_DEBUG
// 单次回环测试：0 未收到结果，1 匹配，-1 收到的测试帧不匹配。
extern volatile int32_t can_debug_result;
HAL_StatusTypeDef CAN_DebugSend(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
```

## C. 建议替换 bsp/src/bsp_can.cpp 的完整内容

用于当前静默回环、RX0 中断练习；每次复位初始化一次；单发送调用者。正式多线程发送需另行设计队列/互斥。filter 全接收只用于入门，电机业务应进一步筛选。

```cpp
#include "bsp_can.hpp"
#include <cstring>

#ifdef CAN_DEBUG
volatile int32_t can_debug_result = 0;
static const uint8_t debug_payload[8] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};
#endif

static bool CAN_IsKnown(CAN_HandleTypeDef* hcan)
{
    return hcan == &hcan1 || hcan == &hcan2;
}

static HAL_StatusTypeDef CAN_ConfigFilter(CAN_HandleTypeDef* hcan)
{
    if (!CAN_IsKnown(hcan)) return HAL_ERROR;

    CAN_FilterTypeDef filter = {};
    filter.SlaveStartFilterBank = 14; // 首次配置前就赋值
    filter.FilterBank = (hcan == &hcan1) ? 0 : 14;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    // ID 和 mask 已清零：不筛选 ID。
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    return HAL_CAN_ConfigFilter(hcan, &filter);
}

static HAL_StatusTypeDef CAN_StartRx(CAN_HandleTypeDef* hcan)
{
    HAL_StatusTypeDef status = HAL_CAN_Start(hcan);
    if (status != HAL_OK) return status;
    return HAL_CAN_ActivateNotification(
        hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

HAL_StatusTypeDef CAN_Init(void)
{
    // 先配置共享过滤器，再启动两路 CAN。
    HAL_StatusTypeDef status = CAN_ConfigFilter(&hcan1);
    if (status != HAL_OK) return status;
    status = CAN_ConfigFilter(&hcan2);
    if (status != HAL_OK) return status;
    status = CAN_StartRx(&hcan1);
    if (status != HAL_OK) return status;
    return CAN_StartRx(&hcan2);
}

HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef* hcan,
    uint32_t id, const uint8_t* data, uint16_t len)
{
    if (!CAN_IsKnown(hcan) || id > 0x7FFU || len > 8U ||
        (len != 0U && data == nullptr))
    {
        return HAL_ERROR;
    }
    if (HAL_CAN_GetState(hcan) != HAL_CAN_STATE_LISTENING)
        return HAL_ERROR;
    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U)
        return HAL_BUSY;

    CAN_TxHeaderTypeDef header = {};
    header.StdId = id;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;

    // 当前 HAL 写邮箱时读取全部 8 字节，短帧也提供完整缓冲区。
    uint8_t payload[8] = {};
    if (len != 0U) std::memcpy(payload, data, len);
    uint32_t mailbox = 0;
    return HAL_CAN_AddTxMessage(hcan, &header, payload, &mailbox);
}

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(
    CAN_HandleTypeDef* hcan)
{
    if (!CAN_IsKnown(hcan)) return;

    // 每次中断最多处理 FIFO 的三个槽位，避免持续流量占住 CPU。
    for (unsigned int i = 0; i < 3U; ++i)
    {
        if (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) == 0U)
            break;

        CAN_RxHeaderTypeDef header = {};
        uint8_t data[8] = {};
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0,
                                &header, data) != HAL_OK)
            break;

        if (header.IDE != CAN_ID_STD || header.RTR != CAN_RTR_DATA ||
            header.DLC > 8U)
            continue;

#ifdef CAN_DEBUG
        if (hcan == &hcan1 && header.StdId == 0x123U)
        {
            can_debug_result =
                (header.DLC == 8U &&
                 std::memcmp(data, debug_payload, 8) == 0) ? 1 : -1;
        }
#endif
        // 后续在这里分发电机反馈，或复制整帧到线程队列。
        // 不保存局部 data/header 的地址，不阻塞打印或等待。
    }
}

#ifdef CAN_DEBUG
HAL_StatusTypeDef CAN_DebugSend(void)
{
    // 仅初始化后调用一次；测试 ID 只用于静默回环。
    can_debug_result = 0;
    return CAN_Transmit(&hcan1, 0x123U, debug_payload, 8);
}
#endif
```

`CAN_Init()` 中途失败时让启动流程进入 Error_Handler；本示例不做运行期热重试。接收回调必须全工程只定义一次。HAL_OK 仅表示发送已提交，收到匹配帧才设置 can_debug_result=1；保持 0 可能是尚未收到或中断链未通。此 volatile 变量仅供单次实验调试观察，不是通用线程同步方案。

## D. main.c 的准确插入位置

不要复制整个 main，不要新增第二个 main。

```c
/* USER CODE BEGIN Includes */
#include "bsp_can.hpp"
/* USER CODE END Includes */
```

确认生成的 `MX_CAN1_Init(); MX_CAN2_Init();` 已在前面执行，再填：

```c
/* USER CODE BEGIN 2 */
if (CAN_Init() != HAL_OK)
{
    Error_Handler();
}
#ifdef CAN_DEBUG
if (CAN_DebugSend() != HAL_OK)
{
    Error_Handler();
}
#endif
/* USER CODE END 2 */
```

此阶段不要再调用会重复启动 CAN 的 bsp_Init，也不要在 while(1) 中轮询读取 FIFO。上板后让程序运行一会儿再暂停，观察 can_debug_result；不要在发送刚返回时就判定接收失败。

## E. 建议的 board/CMakeLists.txt 完整最小版本

适用于当前“只接入 CAN BSP”的阶段。其他 BSP/模块之后按需要加入；CubeMX 子目录继续负责生成代码和 HAL，不要手工重复添加 HAL 源文件。

```cmake
cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 必须用现有 Debug preset，让 ARM 工具链在 project() 前加载。
project(board LANGUAGES C CXX ASM)
add_executable(${PROJECT_NAME})

add_subdirectory(cmake/stm32cubemx)
target_link_libraries(${PROJECT_NAME} stm32cubemx)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bsp/src/bsp_can.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bsp/include
)

option(CAN_DEBUG "Enable CAN loopback debug helper" ON)
if(CAN_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAN_DEBUG)
endif()

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
```

注意：上例的 target_link_libraries 使用普通签名，与生成子目录保持一致；不要在同一目标上混用普通和 PRIVATE/PUBLIC 关键字签名。

工具链文件 `board/cmake/gcc-arm-none-eabi.cmake` 已配置 arm-none-eabi-g++。不要手动把编译器替换为本机 Apple Clang。

## F. 选择正确构建目录，区分编译错误与编辑器红线

由用户运行以下命令；本次助手未运行，避免在项目中创建构建产物：

```sh
cd /Users/jli/f407_lijiali/rm27_training_framework_hw/board
cmake --preset Debug
cmake --build --preset Debug
```

预期构建目录：`board/build/Debug`；预期固件：`board/build/Debug/board.elf`。以构建实际输出为准。根目录旧 build 不需要现在删除。

打开整个项目时，建议用户在 `.vscode/settings.json` 中合并（不要覆盖已有其他设置）：

```json
{
  "cmake.sourceDirectory": "${workspaceFolder}/board",
  "cmake.useCMakePresets": "always",
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

以上假设使用 Microsoft C/C++ 和 CMake Tools。重新选择 Debug 配置预设并执行 CMake: Configure；检查输出实际源目录为 board。若使用 clangd，配置成功后让它读取 board/build/Debug/compile_commands.json，例如合并到 clangd.arguments：

```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/board/build/Debug"
  ]
}
```

按实际使用的语言插件选择，不必同时启用两套诊断。compile_commands.json 中应包含 bsp_can.cpp、HAL/CMSIS 的 -I 路径、-DSTM32F407xx 和 -DUSE_HAL_DRIVER。

- Cannot find source file：先修 basp_can.cpp 拼写。
- can.h: No such file：查生成文件和 board 构建入口/头文件路径。
- const 转换、void 不能作为返回值：同步采用上面的头文件和实现。
- undefined reference：查源文件是否参与构建以及 C/C++ 链接声明是否一致。
- 编译成功但编辑器标红：查语言插件使用的配置/编译数据库。
- SPI/I2C 类型未知：当前未启用相应模块，先通过 CubeMX 配置并生成。

以后上板前，launch.json 的 executable 应指向实际生成的 ELF，例如 `${workspaceFolder}/board/build/Debug/board.elf`；当前两项都仍指向根 build 下的其他名字。

下一步：用户按 B～F 修改并编译，把第一条真实错误发来；下次只追加检查结果，不删除本记录。

---

# 记录 003｜2026-10-03｜简化 CAN 初始化后的编译检查

验证范围：使用 ARM g++ 对当前 bsp_can.cpp 做不落盘的语法检查；另临时通过编译参数补入 cstring，将汇编输出到标准输出核对符号。未修改源码、未生成构建文件、未执行完整链接或硬件测试。

## 结论与修改顺序

1. 实际复现的编译错误：`std::memcpy is not a member of std`。cpp 顶部缺少 `#include <cstring>`。
2. 头文件还是 `CAN_Init(void)`，实现已变成 `CAN_Init(CAN_HandleTypeDef*)`；发送声明是 `const uint8_t*`，实现却是 `uint8_t*`。必须同步。当前编译器把实现当成了另外的 C++ 重载，汇编中实现名字被修饰，DebugSend 却调用 C 符号 CAN_Transmit，后续链接存在 undefined reference 风险。参数名 Id/id 不必一致，但参数类型必须一致；你当前的 Id 大小写已经修好。
3. 初始化第 16 行按句柄选了 0/14，第 27 行又强制设 0：删除后一行，否则 CAN2 过滤器分配错误。合并配置、启动、通知到一个函数完全可以，不必为了拆分而拆分。
4. 发送已补上 AddTxMessage，但 memcpy 前必须检查 len <= 8、指针和 ID。回调为空不会直接导致当前编译失败，但不取走 FIFO 帧可能反复触发中断。
5. 旧的 basp_can.cpp 路径拼写已修好，CAN_DEBUG 已启用。当前 project(...) 默认启用 C 和 CXX，后面的 enable_language(C ASM) 不会关闭 CXX；HAL 已由生成子目录接入。本次实际错误不是“HAL 没同步”。

## 建议完整代码（仅供你手动修改；取代记录 002 的无参数初始化版本）

`bsp/include/bsp_can.hpp`：

```cpp
#ifndef BSP_CAN_H
#define BSP_CAN_H
#include "can.h"
#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef* hcan);
HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef* hcan,
    uint32_t id, const uint8_t* msg, uint16_t len);
#ifdef CAN_DEBUG
extern volatile int32_t can_debug_result;
HAL_StatusTypeDef CAN_DebugSend(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
```

`bsp/src/bsp_can.cpp`：保留一个初始化函数，不再拆辅助初始化函数。适用当前双 CAN、静默回环、单发送调用者练习。

```cpp
#include "bsp_can.hpp"
#include <cstring>

#ifdef CAN_DEBUG
volatile int32_t can_debug_result = 0;
static const uint8_t debug_data[8] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};
#endif

HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef* hcan)
{
    if (hcan != &hcan1 && hcan != &hcan2) return HAL_ERROR;
    CAN_FilterTypeDef filter = {};
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    filter.FilterBank = (hcan == &hcan1) ? 0 : 14;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    // ID 和掩码由 {} 清零：本练习接收所有 ID。
    HAL_StatusTypeDef status = HAL_CAN_ConfigFilter(hcan, &filter);
    if (status != HAL_OK) return status;
    status = HAL_CAN_Start(hcan);
    if (status != HAL_OK) return status;
    return HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef* hcan,
    uint32_t id, const uint8_t* msg, uint16_t len)
{
    if ((hcan != &hcan1 && hcan != &hcan2) || id > 0x7FFU ||
        len > 8U || (len != 0U && msg == nullptr))
        return HAL_ERROR;
    if (HAL_CAN_GetState(hcan) != HAL_CAN_STATE_LISTENING)
        return HAL_ERROR;
    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U)
        return HAL_BUSY;

    CAN_TxHeaderTypeDef header = {};
    header.StdId = id;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;
    // 本地 HAL 读取全部 8 字节，因此短帧也提供 8 字节缓冲区。
    uint8_t payload[8] = {};
    if (len != 0U) std::memcpy(payload, msg, len);
    uint32_t mailbox = 0;
    return HAL_CAN_AddTxMessage(hcan, &header, payload, &mailbox);
}

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan)
{
    if (hcan != &hcan1 && hcan != &hcan2) return;
    // 每次最多取三帧；后续仍有帧时由中断继续处理。
    for (unsigned i = 0; i < 3U; ++i)
    {
        if (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) == 0U) break;
        CAN_RxHeaderTypeDef header = {};
        uint8_t data[8] = {};
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data) != HAL_OK)
            break;
#ifdef CAN_DEBUG
        if (hcan == &hcan1 && header.IDE == CAN_ID_STD &&
            header.RTR == CAN_RTR_DATA && header.StdId == 0x123U)
        {
            can_debug_result = (header.DLC == 8U &&
                std::memcmp(data, debug_data, 8) == 0) ? 1 : -1;
        }
#endif
        // 本练习取出后丢弃非测试帧；以后在这里分发电机反馈。
    }
}

#ifdef CAN_DEBUG
HAL_StatusTypeDef CAN_DebugSend(void)
{
    can_debug_result = 0;
    return CAN_Transmit(&hcan1, 0x123U, debug_data, 8);
}
#endif
```

`hcan1/hcan2` 已在 can.h 声明，不必在 cpp 再写 extern。HAL 回调也已有 HAL 的 C 链接声明，上例显式写 extern "C" 是为了清晰。调试变量需要一个定义，上例已提供。

`board/Core/Src/main.c`：在 USER CODE Includes 中加入：

```c
#include "bsp_can.hpp"
```

在 USER CODE 2 中加入以下代码（必须在生成的 MX_CAN1_Init/MX_CAN2_Init 之后；复位后只执行一次）：

```c
if (CAN_Init(&hcan1) != HAL_OK) Error_Handler();
if (CAN_Init(&hcan2) != HAL_OK) Error_Handler();
#ifdef CAN_DEBUG
if (CAN_DebugSend() != HAL_OK) Error_Handler();
#endif
```

这两次调用分别初始化一路 CAN。当前 main 尚未调用 BSP，属于运行流程待补全，不是 memcpy 编译错误的原因。两路共享过滤器分界保持 14；本例只用于启动阶段，初始化失败不热重试。回环调试仅验证 CAN1，CAN2 仍需独立测试。

下一步：先同步头文件与 cpp，再自行按记录 002 的 board/Debug 入口构建。HAL_OK 仅表示提交发送；上板后观察 can_debug_result=1 才说明本例收到了匹配帧。本次没有硬件验证，也没有宣称完整固件链接通过。
