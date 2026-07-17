#include "mikinolibc/lib.h"

#define MAX_SERVICES 256 // Макс. количество уникальных сервисов
#define MAX_SUBS 4096    // Макс. общее количество подписок (fd <=> сервис)
#define HASH_SIZE 512    // Размер хеш-таблицы (лучше брать степень двойки)
#define MAX_NAME_LEN 64  // Макс. длина имени сервиса

typedef struct {
  char name[MAX_NAME_LEN];
  int head_sub; // Индекс первой подписки в массиве subs
} Service;

// Структура для связи сервиса и fd (элемент списка подписок)
typedef struct {
  int fd;
  int next_sub; // Индекс следующей подписки для этого же сервиса
} Subscription;

// НАША СТАТИЧЕСКАЯ ПАМЯТЬ (ноль динамической памяти!)
Service services[MAX_SERVICES];
int service_count = 0;

Subscription subs[MAX_SUBS];
int sub_count = 0;

// Хеш-таблица: хранит индексы из массива services. -1 означает "пусто"
int hash_table[HASH_SIZE];

unsigned int hash_string(const char *str) {
  unsigned long hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash % HASH_SIZE;
}

// Инициализация (нужно вызвать один раз при старте)
void init_hashtable() {
  for (int i = 0; i < HASH_SIZE; i++)
    hash_table[i] = -1;
  for (int i = 0; i < MAX_SERVICES; i++)
    services[i].head_sub = -1;
}

// Найти или создать сервис. Возвращает внутренний индекс в массиве services
int get_or_create_service(const char *name) {
  unsigned int slot = hash_string(name);

  // Линейное пробирование в случае коллизии
  while (hash_table[slot] != -1) {
    int s_idx = hash_table[slot];
    if (strcmp(services[s_idx].name, name) == 0) {
      return s_idx; // Сервис уже существует
    }
    slot = (slot + 1) % HASH_SIZE;
  }

  // Если сервиса нет, создаем новый
  if (service_count >= MAX_SERVICES) {
    return -1; // Превышен лимит сервисов
  }

  int new_idx = service_count++;
  strncpy(services[new_idx].name, name, MAX_NAME_LEN - 1);
  services[new_idx].head_sub = -1;

  hash_table[slot] = new_idx;
  return new_idx;
}

bool subscribe(int fd, const char *service_name) {
  int s_idx = get_or_create_service(service_name);
  if (s_idx == -1)
    return false; // Нет места под новый сервис

  if (sub_count >= MAX_SUBS)
    return false; // Нет места под подписку

  // (Опционально) Здесь можно проверить, не подписан ли уже этот fd на этот
  // сервис, пройдясь по текущему списку сервиса. В примере пропустим для
  // скорости.

  int new_sub_idx = sub_count++;
  subs[new_sub_idx].fd = fd;

  // Вставка в начало списка (как в обычном Linked List, но на индексах)
  subs[new_sub_idx].next_sub = services[s_idx].head_sub;
  services[s_idx].head_sub = new_sub_idx;

  return true;
}

void unsubscribe_all_from_fd(int fd) {
  // Проходим по всем сервисам
  for (int s_idx = 0; s_idx < service_count; s_idx++) {
    int current_sub = services[s_idx].head_sub;
    int prev_sub = -1;

    // Идем по цепочке подписок текущего сервиса
    while (current_sub != -1) {
      // Запоминаем индекс следующей подписки сразу,
      // так как текущую мы можем переместить/изменить
      int next_sub = subs[current_sub].next_sub;

      if (subs[current_sub].fd == fd) {
        // 1. Исключаем элемент из цепочки данного сервиса
        if (prev_sub == -1) {
          // Удаляем голову списка
          services[s_idx].head_sub = next_sub;
        } else {
          // Удаляем элемент из середины или конца
          subs[prev_sub].next_sub = next_sub;
        }

        // 2. Утилизируем место в статическом массиве subs.
        // Если удаляемый элемент не был самым последним в массиве,
        // перемещаем самый последний элемент на его место.
        int last_idx = sub_count - 1;
        if (current_sub != last_idx) {
          // Копируем данные последнего элемента на место удаленного
          subs[current_sub] = subs[last_idx];

          // Теперь нужно найти, какой сервис владел этим перемещенным last_idx,
          // и обновить в его цепочке индекс с last_idx на current_sub.
          for (int s_search = 0; s_search < service_count; s_search++) {
            if (services[s_search].head_sub == last_idx) {
              services[s_search].head_sub = current_sub;
              break;
            }
            int scan = services[s_search].head_sub;
            bool found = false;
            while (scan != -1) {
              if (subs[scan].next_sub == last_idx) {
                subs[scan].next_sub = current_sub;
                found = true;
                break;
              }
              scan = subs[scan].next_sub;
            }
            if (found)
              break;
          }

          // Так как на место current_sub прилетел новый элемент (бывший
          // последний), и его тоже нужно проверить (вдруг это тот же fd), мы не
          // сдвигаем prev_sub, а current_sub проверится заново.
          current_sub = next_sub;
          sub_count--;
          continue;
        }

        sub_count--;
        // Переходим к следующему элементу цепочки
        current_sub = next_sub;
      } else {
        // Если fd не совпал, просто двигаемся дальше по списку
        prev_sub = current_sub;
        current_sub = next_sub;
      }
    }
  }
}

void send_to_all(const char *service_name, char *body, size_t body_size) {
  unsigned int slot = hash_string(service_name);
  int s_idx = -1;

  // Ищем сервис в хеш-таблице
  while (hash_table[slot] != -1) {
    int idx = hash_table[slot];
    if (strcmp(services[idx].name, service_name) == 0) {
      s_idx = idx;
      break;
    }
    slot = (slot + 1) % HASH_SIZE;
  }

  if (s_idx == -1) {
    print(&STDERR_IO, "Сервис ", service_name,
          " не найден или у него нет подписчиков.\n");
    return;
  }

  // Проходим по всем fd, подписанным на этот сервис
  int current_sub = services[s_idx].head_sub;
  while (current_sub != -1) {
    int client_fd = subs[current_sub].fd;

    STDOUT_IO.fd = client_fd;
    print(&STDOUT_IO, service_name, ":");
    print_array(&STDOUT_IO, body, body_size);
    print(&STDOUT_IO, _endl);
    STDOUT_IO.fd = STDOUT_FILENO;

    // Переходим к следующему подписчику
    current_sub = subs[current_sub].next_sub;
  }
}
