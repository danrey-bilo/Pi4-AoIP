# Проверка разделённого репозитория

Дата: 24 сентября 2026. Проверены исходники отдельного платформенного проекта
с общей библиотекой AoIP-lib, подключённой через `AOIP_SOURCE_DIR`.

Стенд: Raspberry Pi 4 Model B Rev 1.5, Debian ARM64, GCC 14.2.0,
ядро `6.18.50+rpt-rpi-v8-rt`, `realtime=1`, `isolated=2-3`.
Все сборки и тесты запущены через `taskset -c 0,1`.

| Проверка | Результат |
|---|---|
| Release CMake | `aoip_core`, `aoip_peer`, `aoip_rpi4`, `aoip_peer_rpi4`, пример: собраны |
| CTest | `host_info`, `core`: 2/2 |
| Экспорт и установка CMake SDK | AoIP и Pi4AoIP установлены в отдельный prefix |
| Внешний потребитель из `tests/consumer` | `find_package(Pi4AoIP 2.1 CONFIG REQUIRED)`, сборка, тест: 1/1 |
| `aoip_peer_rpi4 --check` | Pi4 и RT определены; `network_cpus=0,1`, `effects_cpus=2,3`, Ethernet найден |
| `packaging/debian/build-deb.sh` | Собраны `piaoip-rpi4_2.1.0-1_arm64.deb` и `piaoip-sdk_2.1.0-1_arm64.deb` |

Новые пакеты в этой проверке не устанавливались поверх работающей службы.
Сборка DEB не равнозначна проверке install/upgrade/remove. Для полного цикла
предусмотрен отдельный тест [package_lifecycle.py](../tests/package_lifecycle.py);
он изменяет установку и выполняется на подготовленном тестовом устройстве.

Длительный аудиопрогон после разделения репозиториев здесь не заявляется.
Исторические результаты прежней сборки и план измерений — в [TESTING.md](TESTING.md).
