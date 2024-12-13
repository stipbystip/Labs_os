import threading
import queue
import time

event_queue = queue.Queue()
condition = threading.Condition()


def provider():
    """Генерирует события и добавляет их в очередь."""
    event_id = 1
    while True:
        with condition:
            print(f"Поставщик: создаю событие {event_id}.")
            event_queue.put(event_id)
            condition.notify_all()
            event_id += 1
        time.sleep(0.5)


def consumer(consumer_id):
    """Обрабатывает события из очереди."""
    while True:
        with condition:
            while event_queue.empty():
                print(f"Потребитель {consumer_id}: жду события...")
                condition.wait()
            event = event_queue.get()
            print(f"Потребитель {consumer_id}: обработал событие {event}.")
        time.sleep(1)



thread_provider = threading.Thread(target=provider, daemon=True)


thread_consumers = [
    threading.Thread(target=consumer, args=(i,), daemon=True)
    for i in range(1, 4)
]


thread_provider.start()
for thread in thread_consumers:
    thread.start()

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\nЗавершение работы...")