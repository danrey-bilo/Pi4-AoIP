# API платформы Pi4

`find_package(Pi4AoIP 2.1 CONFIG REQUIRED)` и `Pi4AoIP::platform`.

```cpp
#include <aoip/rpi4.hpp>
#include <thread>

std::string error;
if (!aoip::rpi4::initialize(error)) {
    // Сообщить error и завершить запуск сервиса.
    return 2;
}
std::thread receiver([] {
    aoip::peer::enter_thread_role(aoip::peer::ThreadRole::receive);
    // Собственный приёмный цикл. Ошибка affinity/FIFO завершает процесс.
});
receiver.join();
```

`initialize()` вызывается в главном потоке ДО создания остальных потоков:
проверяет модель и RT-ядро, ограничивает главный поток CPU0/CPU1 и устанавливает
политику ролей peer. Потомки наследуют ограничение до своего назначения роли.
Не заменяйте hook во время работы потоков. Не вызывайте `initialize()` из потока
аудиоэффектов, которому нужны CPU2/CPU3.

| API | Назначение |
|---|---|
| `inspect_host()` | Читает модель, версию ядра, признак PREEMPT_RT и список isolated CPUs; ничего не изменяет |
| `initialize(error)` | Проверяет Pi 4/RT/четыре online CPU и устанавливает политику CPU0/CPU1; `false` и текст ошибки при отказе |
| `configure_thread(role)` | Текущий поток: RX → CPU0/FIFO70; TX → CPU1/FIFO70; control/reporter → CPU1/SCHED_OTHER; `false` при ошибке |
| `find_wired_endpoint(name, endpoint, error)` | Выбирает один активный физический Ethernet IPv4; пустое имя означает автоматический выбор; исключает Wi-Fi и неоднозначный выбор |

Для FIFO70 необходим `LimitRTPRIO=80` у systemd-службы. Пример готовой службы
в [pi-aoip.service](https://github.com/danrey-bilo/AoIP-debug-tool/blob/main/packaging/debian/pi-aoip.service) закрытого репозитория разработчиков. Она использует отдельного пользователя
`piaoip`, `CPUAffinity=0 1` и каталог состояния `/var/lib/piaoip`.
Библиотека не меняет настройки IRQ, boot cmdline, сеть, Wi-Fi, Bluetooth, USB или
GPIO. Ограничение касается потоков PiAoIP; распределение системных IRQ задаётся ОС.
