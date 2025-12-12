<div align="center" markdown="1">
  <img src=".github/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>

<h1 align = "center">🌟LilyGo-Modem-Series🌟</h1>

[![PlatformIO CI](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/actions/workflows/platformio.yml/badge.svg)](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/actions/workflows/platformio.yml)

# News

- 2025/09/19 : Added SIM7070G support, compatible with SIM7080G
- 2025/08/07 : Completed support for SIM7080G series
- 2025/07/26 : Add **T-SimShield** support, please refer to the [document](./docs/en/SimshieldUsageGuide/README.md) here for quick setup
- 2025/07/10 : The example has completed most of the support for SIM7000G. For non-encrypted MQTT, please refer to the description above the example. Some of them are not supported.
- 2025/07/09 : Completed support for SIM7600 series
- The TinyGSM used in the example is a [fork](https://github.com/lewisxhe/TinyGSM) to support A7670,A7608,SIM7672G,SIM7670G, If you use the master branch, the compilation will not go smoothly

> \[!IMPORTANT]
> If you encounter a problem during use, first check whether the modem's current firmware version is the latest, and then search for [issues](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/issues) to see if there are similar issues.
>

## ESP32 Version Get Started

<table style="width: 100%; border-collapse: collapse; text-align: center; border: 0; border-spacing: 0;">
  <tr style="border: 0;">
    <!-- T-A7670X -->
    <td style="padding: 10px 20px; width: 33.33%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/a7670-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/Q334-T-A7670E-ESP32.png" alt="T-A7670X-ESP32" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-A7670X Quick Start</div>
      </a>
    </td>
    <!-- T-A7608X -->
    <td style="padding: 10px 20px; width: 33.33%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/a7608-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/Q344-T-A7608-ESP32.png" alt="T-A7608X-ESP32" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-A7608X Quick Start</div>
      </a>
    </td>
    <!-- T-SIM7000G -->
    <td style="padding: 10px 20px; width: 33.33%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/sim7000-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/Q141-SIM7000G-ESP32.png" alt="T-SIM7000G-ESP32" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-SIM7000G Quick Start</div>
      </a>
    </td>
  </tr>
</table>

<table style="width: 100%; border-collapse: collapse; text-align: center; border: 0; border-spacing: 0; margin: 20px 0;">
  <tr style="border: 0;">
    <!-- T-SIM7070G-ESP32 -->
    <td style="padding: 10px 20px; width: 33.33%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/sim7070-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/Q323-T-SIM7070G-ESP32.png" alt="T-SIM7070G-ESP32" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-SIM7070G Quick Start</div>
      </a>
    </td>
    <!-- T-SIM7600X -->
    <td style="padding: 10px 20px; width: 33.33%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/sim7600-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/H503-T-SIM7600G-ESP32.png" alt="T-SIM7600X" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-SIM7600X Quick Start</div>
      </a>
    </td>
    <!-- T-Call-A7670X -->
    <td style="padding: 10px 20px; width: 33.33%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/t-call-a7670x/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/H700-T-Call-A7670-ESP32.png" alt="T-Call-A7670X" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-Call-A7670X Quick Start</div>
      </a>
    </td>
  </tr>
</table>

<table style="width: 100%; border-collapse: collapse; text-align: center; border: 0; border-spacing: 0; margin: 20px 0;">
  <tr style="border: 0;">
    <!-- T-PCIE-Series -->
    <td style="padding: 10px 20px; width: 50%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/pcie-series-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/Q415-PCIE-ESP32.png" alt="T-PCIE-Series" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-PCIE-Series Quick Start</div>
      </a>
    </td>
    <!-- T-InterNetCom -->
    <td style="padding: 10px 20px; width: 50%; border: 0; background: transparent;">
      <a href="./docs/en/esp32/t-internet-com-esp32/REAMDE.MD" style="text-decoration: none; display: block;">
        <img src="./images/product/png/Q329-T-Internet-COM.png" alt="T-Internet-COM" width="200" style="border: 0;"/>
        <div style="color: #FFFF; font-weight: 600; margin-top: 8px;">T-Internet-COM Quick Start</div>
      </a>
    </td>
</table>

## Model Comparison

- ⁉️ **[Differences between different versions and models](./docs/model_comparison.md)**

## Modem firmware bug summary

- 🐞 **[Modem firmware bug summary](./docs/simcom_firmware_bug.md)**
