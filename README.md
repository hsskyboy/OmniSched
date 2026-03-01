# OmniSched | 全域靈動調度 🚀

[![Android 12+](https://img.shields.io/badge/Android-12%2B-3DDC84?style=flat-square&logo=android)](#)
[![Root Required](https://img.shields.io/badge/Root-Magisk%20v23.0%2B%20%7C%20KernelSU-orange?style=flat-square)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg?style=flat-square)](#)

**OmniSched** 是一款重構安卓調度邏輯的現代化 Android 底層優化模組。我們摒棄了傳統模組中「寫死核心數」與「綁定特定處理器」的過時做法，透過**全域動態運算**，將旗艦級的效能與能耗比帶給每一台 Android 12+ 設備。

## 💡 為什麼選擇 OmniSched？

市面上多數的調度模組往往針對特定機型（如純 8 核或純高通平台）進行硬編碼（Hardcode）。一旦刷入不同架構的設備（如聯發科、獵戶座，或未來的異構多核），輕則無效，重則導致系統卡死、發熱耗電。

OmniSched 的誕生就是為了解決這個痛點：**一次刷入，全平台自適應。**

## 🌟 核心黑科技

* **🧠 智能 SoC 動態嗅探**
    開機瞬間自動偵測底層硬體。若為高通 (Qualcomm) 設備，自動解鎖專屬 QTI 底層優化與 Adreno 最佳調度 (`msm-adreno-tz`)；若為聯發科 (MediaTek) 或獵戶座 (Exynos) 等通用平台，則無縫切換至泛用型 Mali 動態調度。
* **⚙️ Cpuset 動態幾何分配**
    不再寫死 `0-7` 或 `0-3`。腳本會自動讀取設備的極限核心數，智能劃分「大、中、小」核梯度。讓前台遊戲 (Top-app) 獨占最強算力，後台進程嚴格鎖死於省電小核。
* **🎮 底層渲染深度劫持 (A12+ 專屬)**
    在 `post-fs-data` 階段提前接管 `ro.` 唯讀屬性。全域強制開啟 Vulkan 引擎與 SkiaVK 渲染後端，並啟用「髒區域渲染 (Dirty Regions)」，僅重新繪製畫面有變動的部分，大幅降低 GPU 渲染負載。
* **⚖️ Schedutil 負載感知**
    強制統一 CPU 調度器為 `schedutil`，沒有無腦鎖滿頻的發熱問題。
* **✨ 零「安慰劑」代碼**
    不含 Android 4.0-8.0 時代遺留的無效代碼，腳本極簡輕量，執行效率極高。

## 📱 系統相容性

* **系統版本：** Android 12 及以上 (API 31+)。
* **處理器環境：** Qualcomm, MediaTek, Exynos 等。
* **Root 環境：** 支援 Magisk (v23.0+) / KernelSU / APatch 等。

## 📥 安裝與使用

1. 在 Release 頁面下載最新版的 `OmniSched-xxx.zip`。
2. 打開 Magisk / KernelSU / APatch 的「Modules」頁面。
3. 選擇「從本機安裝」，選取下載的 ZIP 壓縮檔。
4. 觀察安裝介面的設備偵測提示，確認無誤後重啟手機。

## ⚠️ 免責聲明

本模組涉及 Android 底層 Cpuset 與 GPU/CPU 調度調整。雖然經過了嚴格的動態相容性設計，但刷機仍有風險。因使用本模組導致的任何數據遺失或設備異常，開發者不承擔任何責任。**建議刷入前做好重要數據備份。**
