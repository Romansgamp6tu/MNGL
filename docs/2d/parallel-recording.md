# Параллельная запись 2D-кадра

Для CPU-тяжёлой общей 2D-сцены кадр можно записывать в нескольких независимых контекстах. Каждый worker получает собственные буферы команд; после завершения главный поток объединяет их в нужном порядке. Эти контексты готовят CPU-данные и не вызывают GPU — итоговый кадр отправляется одним обычным 2D-проходом. Для независимых offscreen-целей, где worker также кодирует и отправляет свой GPU-проход, см. следующий раздел.

```cpp
#include <future>
#include <vector>

std::vector<mngl::sprite> sprites; // Заранее заполните спрайтами сцены.
constexpr std::size_t worker_count = 2;
const std::size_t middle = sprites.size() / worker_count;

auto frame = window.frame();
auto first = frame.create_recording_context(middle);
auto second = frame.create_recording_context(sprites.size() - middle);

auto record_range = [&](mngl::two_d::recording_context& context, std::size_t begin, std::size_t end)
{
	for (std::size_t index = begin; index < end; ++index)
		context.draw(sprites[index]);
	context.finish();
};

auto first_job = std::async(std::launch::async, [&] { record_range(first, 0, middle); });
auto second_job = std::async(std::launch::async, [&] { record_range(second, middle, sprites.size()); });
first_job.get();
second_job.get();

frame.merge(std::move(first));
frame.merge(std::move(second));
frame.display();
```

`merge()` задаёт порядок отрисовки, а не порядок завершения потоков. Это важно для alpha blending и hit testing. Каждый контекст должен использоваться только одним потоком одновременно; не передавайте worker-ам родительский `frame_2d`. До `merge()` контекст нужно завершить вызовом `finish()`.

Перед запуском worker-ов подготовьте ресурсы, которые при обычном рисовании создаются лениво. Встроенные shader-спрайты и текст поддерживают `frame.prepare()`:

```cpp
auto frame = window.frame();
frame.prepare(shader_sprite);
frame.prepare(title_text);

// После этого можно создавать recording_context и запускать worker-ы.
```

Подготовка shader-спрайта создаёт его automatic material/instance, а подготовка текста растеризует нужные glyph-ы и размещает их в font atlas. Это позволяет worker-ам только читать уже созданные ресурсы. Пользовательский drawable может переопределить protected `prepare(frame_2d&) const` и вызвать `frame.prepare()` для содержащихся в нём объектов; его остальные изменяемые ресурсы тоже нужно подготовить до параллельной записи.

Пока worker-контексты записывают команды, не изменяйте и не уничтожайте используемые текстуры, шрифты, материалы, drawable-объекты и их данные. Окно, target и renderer должны жить до завершения и объединения всех контекстов. Создание GPU-ресурсов, загрузка glyph-ов, изменение frame view, `merge()` и `display()` выполняются на главном потоке. Незавершённые контексты нельзя объединить или использовать для завершения кадра; если `display()` вызван слишком рано, кадр остаётся активным — сначала завершите и объедините контексты, затем повторите вызов. Если контекст нужно отбросить, уничтожьте его до повторного `display()` — записанные в него команды будут потеряны.

Параллельная запись уменьшает CPU-время подготовки больших сцен, но не ускоряет GPU-проход и добавляет стоимость объединения буферов. Для коротких сцен обычный `frame.draw()` остаётся предпочтительным.

## Независимые offscreen-цели

Если сцены не зависят друг от друга, можно параллельно отрисовать их в разные текстуры, а затем собрать результат в окне. `sprite::borrowed()` здесь подчёркивает заимствование: исходные текстуры и цели должны жить и оставаться по тому же адресу до завершения всех worker-кадров и композиции. Сначала создайте все frame scope на главном потоке; каждый scope и его target должна использовать только одна задача:

```cpp
#include <future>

mngl::texture left_target(512, 512);
mngl::texture right_target(512, 512);
auto left_content = mngl::sprite::borrowed(left_source, {0.0f, 0.0f, 1.0f, 1.0f});
auto right_content = mngl::sprite::borrowed(right_source, {0.0f, 0.0f, 1.0f, 1.0f});

auto left_frame = left_target.frame();
auto right_frame = right_target.frame();
left_frame.prepare(left_content);
right_frame.prepare(right_content);

auto left_job = std::async(std::launch::async,
                           [&]
                           {
	                           left_frame.draw(left_content);
	                           left_frame.display();
                           });
auto right_job = std::async(std::launch::async,
                            [&]
                            {
	                            right_frame.draw(right_content);
	                            right_frame.display();
                            });
left_job.get();
right_job.get();

auto left_result = mngl::sprite::borrowed(left_target, {0.0f, 0.0f, 0.5f, 1.0f});
auto right_result = mngl::sprite::borrowed(right_target, {0.5f, 0.0f, 0.5f, 1.0f});
auto composite = window.frame();
composite.draw(left_result);
composite.draw(right_result);
composite.display();
```

Здесь `frame_scope::display()` кодирует и отправляет GPU-команды на том же worker-потоке, где scope использовался. Это ограничение SDL: command buffer должен быть отправлен потоком, который его получил, поэтому переносить его на главный поток нельзя. Главный поток собирает результаты через `future::get()` и начинает композицию только после завершения всех задач. Порядок независимых offscreen-проходов не важен; порядок `draw()` при композиции задаёт итоговый порядок смешивания прозрачности. Если одна цель использует результат другой, между задачами нужна явная зависимость — параллельный пример выше рассчитан именно на независимые сцены.

Для каждой одновременно используемой цели renderer выбирается отдельно; после завершения кадра он может быть переиспользован. Поэтому первый запуск с новым максимумом параллелизма создаёт дополнительные renderer и связанные GPU-ресурсы. Как и для worker-контекстов одного кадра, заранее вызывайте `prepare()` для shader-спрайтов, текста и других ресурсов с ленивой подготовкой; не меняйте и не уничтожайте общие ресурсы до завершения worker-ов. Если несколько задач используют один font atlas, его обновление и отправка GPU-команд сериализуются на время финализации этих кадров; сами draw-вызовы остаются независимыми.

