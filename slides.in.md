<!-- theme: mws -->
<!-- _class: title -->
<!-- title: Async C++ -->

# АСИНХРОННЫЙ <br/> КОД В C++
## Анатолий Щеглов <br/> ООО "МТС Веб Сервисы"

---
<!-- paginate: true -->

# Содержание

- Синхронный код
- Коллбэки
- Корутины
- `std::execution`

---

# Введение - Синхронный код

```c++
struct socket_t;

void sync_read(socket_t socket,
               byte* buffer, size_t buffer_size);
```

- Читает в буфер.
- Возвращает управление только когда закончит чтение.

---

# Введение - Асинхронный код

```c++
using callback_t = void(void* arg);

void async_read(socket_t socket,
                byte* buffer, size_t buffer_size,
                callback_t* callback, void* callback_arg);

for (;;) {
  run_completion_callbacks(); 
  wait_completions();
}
```
- Сразу возвращает управление.
- Читает в буфер в фоне.
- После завершения фонового чтения,
  `run_competion_callbacks()` выполняет `callback(callback_arg)`.

---

# Введение - История

- 2009 год - `boost::future<T>`, `.then()`.
- С++11 - блокирующий `std::future<T>::get()` без `.then()`.
- 2012 год - N3328 Resumable Functions.
- 2013-2015 годы - превью корутин в MS Visual C++ 2013 и 2015.
- 2018 - Coroutines TS (aka Гор-рутины, в честь Гора Нишанова).
- С++20 - корутины в стандарте.
- С++26? - `std::execution`

---

# Задача чтения данных динамического размера

### Байты на проводе
```
.------+-------.------+-------.-------.
| len1 | data1 | len2 | data2 | len=0 |
'------+-------'------+-------'-------'
```

### Алгоритм
```
  ,-<---------------------------------------<--.
  `-> [read len] --> [read data] -> [process] -'
                 \
                  `--> done if len==0
```

### Функция обработки
```c++
void Process(vector<byte> data);
```

---

# Синхронный код - API чтения

```c++
!!! include 1.1-sync.cc API
```

- Читает от `1` до `buf.size()`
- Возвращает размер прочитанного

---

# Синхронный код - чтение всего буфера

```c++
!!! include 1.1-sync.cc ReadAll
```

---

# Синхронный код - обработка соединения

```c++
!!! include 1.1-sync.cc HandleConnection
```

---

# Синхронный код - хвостовая рекурсия

```c++
!!! include 1.2-sync-rec.cc ReadAll
```
### vs обычный цикл
```c++
!!! include 1.1-sync.cc ReadAll
```

---

# CPS - continuation passing style

## aka коллбэки

- Функции не возвращают результат через `return result;`
- Функции передают результат в коллбэк - `return callback(result);`

---

# CPS - API чтения

```c++
!!! include 2.1-cps-nested-lambda.cc API
```

- Читает в `buf`.
- Вызывает `callback` с размером прочитанного.

---

# CPS - чтение всего буфера - рекурсия

```c++
!!! include 2.1-cps-nested-lambda.cc ReadAll
```
- Рекурсия вместо цикла.
- Неограниченная рекурсия если `ReadSome` вызывает коллбэк синхронно.

---

# CPS - чтение всего буфера - аллокации

```c++
!!! include 2.1-cps-nested-lambda.cc ReadAll
```
- Всё состояние в замыкании - `[=]`,
- `function<> callback` не может поместиться в `function<>` без аллокации.

---

# CPS - обработка соединения

```c++
!!! include 2.1-cps-nested-lambda.cc HandleConnection
```
- `data_size` и `data` должны иметь стабильный адрес на время чтения 


---

# CPS + struct - чтение всего буфера

```c++
!!! include 2.2-cps-no-alloc.cc ReadAll
```
- `[this]` - маленькое замыкание, без аллокаций в `function<>`

---

# CPS + struct - обработка соединения

```c++
!!! include 2.2-cps-no-alloc.cc HandleConnection
```

---

# CPS + синхронный возврат

```c++
!!! include 2.3-cps-sync-res.cc API
```

```c++
!!! include 2.3-cps-sync-res.cc ReadAll
```

- Компилятор может распознать хвостовую рекурсию при `-O2`.
  `Start -> (inline) OnRead -> Start`

---

# Корутины

- Корутина - это функция с `co_return` или `co_await`.
- Корутина возвращает некоторый тип `Future`,
  у которого есть тип `Future::promise_type`.

```c++
struct Future { struct promise_type { ... }; };

Future coro()     { co_return; }     // корутина
Future not_coro() { return coro(); } // не корутина
```

---

# Корутины - API чтения

```c++
!!! include 3.1-coro.cc API
```

---

# Корутины - чтение всего буфера

```c++
!!! include 3.1-coro.cc ReadAll
```

### vs синхронная версия:
```c++
!!! include 1.1-sync.cc ReadAll
```

---

# Корутины - обработка соединения

```c++
!!! include 3.1-coro.cc HandleConnection
```

---

# Корутины - устройство

```c++
Future coro() {
  // co_return co_await other();
  struct CoroutineState {
    Future::promise_type p_;
    Future other_fut_;
    int state_{0};
    void resume() { switch (state_) {
      case 0:
        other_fut_ = other();
        if (!other_fut_.await_ready()) {
          state_ = 1;
          return other_fut_.await_suspend(std::coroutine_handle{this});	      
        }
      case 1:
        p_.return_value(other_fut_.await_resume());	
    }}
  };
  auto* s = new CoroutineState{};
  s->resume();
  return s->p_.get_return_object();
}
```

---

# Корутины - свой код аллокации

```c++
Future coro(span<byte> mem) {
  co_return 1;
}

struct Future {
  struct promise_type {
    template <typename... OtherArgs>
    static void* operator new(size_t count, span<byte> mem, OtherArgs&&...) {
      assert(count <= mem.size());
      return mem.data();
    }
  };
};
```

- Размер "CoroutineState" не `constexpr`, он становится известен только после всех оптимизаций.

---

# Senders and Receivers

## aka `std::execution`, `C++26 [exec]`, `[p2300]`.

- "Sender" - фабрика асинхронной операции.
- "Receiver" - принимает результат асинхронной операции,
  почти как коллбэк.
- Результат операции - это успех, ошибка, или остановка (отмена).

```c++
struct Receiver {
  void set_value(V...);
  void set_error(E);
  void set_stopped();
};
```

---

# Senders and Receivers - пример

```c++
!!! include 4.1-snr-custom.cc Def
```

```c++
!!! include 4.1-snr-custom.cc Use
```
---

# Senders and Receivers - стандартные

- `just(value)` - сендер, который сразу отдает `value`.
- `then(sender_of<X>, [](X)->Y {})` - добавляет коллбэк сендеру,
  "меняет" результат сендера.
- `let_value(sender_of<X>, [](X)->sender_of<Y> {})` - возвращает новую асинхронную операцию, передавая в неё результат сендера.

```c++
!!! include 4.2-snr-basic.cc Basic
```

---

# Senders and Receivers - any

```c++
!!! include 4.3-snr-any.cc Any
```

- В функции два return с выражениями разных типов, но у функции может быть только один тип результата.
- Используем стирание типов.
- **Стирание типов может приводить к аллокации.**

---

<!--
_class: black
_paginate: false
-->

Directed by
ROBERT B. WEIDE

---

# Senders and Receivers - API чтения

```c++
!!! include 4.4-snr.cc API
```

---

# Senders and Receivers - чтение всего буфера

```c++
!!! include 4.4-snr.cc ReadAll
```

---

# Выводы

- Корутины хороши если аллокации не проблема.
- Коллбэки не требуют сильно больше кода.
- `std::execution` предлагает интересные идеи,
  (в т.ч. отдельные set_value/set_error вместо коллбэка),
  но в текущей реализации есть места для улучшений.

---
<!-- _class: last -->

# Вопросы?

<table>
<tr>
<td>
<p>Исходники кода на слайдах</p>

![width:400px](github-repo-qr.png)
https://github.com/ascheglov/cxx-async-io/
</td>
<td>
<p>Чат про С++</p>

![width:400px](tg-pro-cxx-qr.png)
</td>
</tr>
</table>
