# BLE ADC Multiple Peripheral Code

---

## Description

This DA14531 code demonstrates how a single central can be connected to more than one peripheral. The original DA14531 code examples self-limit to a single peripheral for stability in simple example cases. The DA14531 architecture is able to support up to 3 connections and the DA14585 up to 8 simultaneous connections. Additionally, we also demonstrates how one could keep different characteristic values depending on different connections.

## Usage

We compile the code and flash through the Keil μVision environment, but others can be utilized based on your preference. Simply run the code through your preferred environment and use an external BLE scanning tool (such as the LightBlue app on iOS) to see characteristics and other important BLE information to debug whatever implementation you are aiming for.
