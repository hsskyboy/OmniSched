# OmniSched | 全域灵动调度 🚀

![Android](https://img.shields.io/badge/Android-12%2B-3DDC84?style=flat-square&logo=android)
![C++](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=c%2B%2B)
![Magisk](https://img.shields.io/badge/Magisk-23.0%2B-000000?style=flat-square)
![License](https://img.shields.io/badge/License-Mulan_PubL_2.0-blue?style=flat-square)

OmniSched 是一款重构安卓调度逻辑的现代化 Android 底层优化模组。v2.0 迎来史诗级重构，我们彻底抛弃了传统模组中「单次 Shell 脚本执行」与「写死核心数」的过时做法，全面转向 **原生 C++ 守护行程 (Daemon)**。透过全域动态运算与硬体嗅探，将旗舰级的效能与能耗比带给每一台设备。

## 💡 为什么选择 OmniSched？
市面上多数的调度模组往往针对特定机型（如纯 8 核或纯高通平台）进行硬编码。一旦刷入不同架构的设备，轻则无效，重则导致系统卡死、发热耗电。
* OmniSched 的诞生就是为了解决这个痛点：**一次刷入，全平台自适应**。

## 🌟 核心黑科技
- ⚡ **原生 C++ 极客守护行程**
  - 舍弃 Shell 脚本，改用 LTO 最佳化与纯 C/POSIX API 重构的二进位档。极低功耗背景常驻，每 15 分钟自动巡检，确保调度设定不被系统杀后台或覆盖。
- 🧠 **动态调速器嗅探 (Dynamic Governor Sniffing)**
  - 不再无脑写死 `schedutil`！程式会自动读取内核节点的可用列表，智能配对当前环境的最佳解，绝不与你的内核打架。
- ⚙️ **Cpuset 动态几何分配**
  - 自动读取设备核心架构，智能划分「大、中、小」核梯度。让前台应用 (Top-app) 独占最强算力，后台进程严格锁死于省电小核。
- 🎮 **底层渲染深度劫持 (A12+ 专属)**
  - 在 `post-fs-data` 阶段提前接管渲染属性。全域强制开启 Vulkan 引擎与 SkiaVK 渲染，大幅降低 GPU 负载。

## 📱 系统相容性
* **系统版本**：Android 12 及以上 (API 31+)。
* **Root 环境**：支援 Magisk (v23.0+) / KernelSU / APatch。

## 📥 安装与使用

1. 若曾安装过 OmniSched v1.x 版本，请先在 Root 管理器中卸载。
2. 在 Release 页面下载最新版的 `OmniSched-v2.x.x.zip`。
3. 透过root管理器选择「从本机安装」。
4. 观察安装介面的设备侦测提示，重启手机启用模组。

## ⚖️ 律法与许可证 (License)
本项目采用 木兰公共许可证第2版 (Mulan PubL-2.0) 授权：
1. 强制要求下游开源
任何基于本项目的修改、二次开发、代码引用，其衍生项目必须同样采用 Mulan PubL-2.0 或同等 Copyleft 协议开源。禁止任何形式的闭源二改。
2. 必须保留署名与原出处
在您的模组安装界面（UI）、README 说明、代码注释中，必须显著标注原作者署名及本项目仓库链接。严禁抹除作者信息或伪装成自研。
3. 禁止商业牟利与欺诈
 - 禁止倒卖： 严禁将本模组打包在咸鱼、淘宝等平台付费出售。
 - 禁止收费进群： 严禁将本模组作为付费群、付费频道的「独家内容」。
 - 禁止欺骗： 若发现下游项目违反上述条款，原作者有权对违规项目进行公开披露。
> 代码可以免费分享，但劳动成果不容剽窃。尊重开源，遵守约定，否则不要使用本专案程式码。

⚠️ 免责声明
本模组涉及 Android 底层 Cpuset 与 GPU/CPU 调度调整。刷机有风险，因使用本模组导致的任何数据遗失或设备异常，开发者不承担任何责任。建议刷入前做好重要数据备份。
