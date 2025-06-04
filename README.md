# Arduino Sine Wave Generator

Tento repozitář obsahuje jednoduchý příklad generátoru sinusové vlny pro
Arduino Uno. Výstup je tentokrát generován pomocí externího DAC
převodníku a jeho analogový signál připojte na pin **A0**.

## Obsah
- `sine_wave_generator/sine_wave_generator.ino` – hlavní Arduino sketch

## Použití
1. Otevřete soubor `.ino` v Arduino IDE.
2. Nahrajte projekt na Arduino Uno.
3. Výstup DAC připojte k pinu A0 a společné zemi.

Výstupní frekvence vlny je nastavena na 1\xa0kHz. Pokud potřebujete
jinou hodnotu, upravte konstantu `outputFreq` ve zdrojovém kódu.
