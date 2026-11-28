# Граф кадра

Обычному 2D-коду граф не нужен: используйте `window.frame()` и `frame.draw(...)`. `mngl::gpu::frame_graph` — отдельный продвинутый слой для сценариев, где compute- и render-проходы обмениваются GPU-ресурсами.

Граф создаёт временные текстуры и буферы и владеет ими до своего уничтожения. Проходы объявляют чтение и запись; по этим объявлениям строится порядок выполнения. Для transient-ресурса нужен записывающий проход, который можно поставить до первого чтения; отсутствие производителя приводит к ошибке. Внешние ресурсы подключаются через `import_texture()` и `import_buffer()` и остаются во владении вызывающего кода. Все GPU-проходы записываются в один command buffer, который граф отправляет после завершения проходов.

## Compute, затем обычная 2D-отрисовка

Этот пример записывает compute-команду в transient-текстуру, а затем рисует её обычным 2D renderer-ом в окно. Оконный кадр намеренно остаётся вне графа:

Положите HLSL-файл из раздела [compute](compute.md#hlsl-texture-writing) в `shaders/fill.hlsl`. Проект должен ссылаться на `MNGL::MNGL`, как в [инструкции подключения](../getting-started/using-mngl.md).

```cpp
#include <MNGL/compute_pass.hpp>
#include <MNGL/frame_graph.hpp>
#include <MNGL/input.hpp>
#include <MNGL/render_window.hpp>
#include <MNGL/renderer2d.hpp>
#include <MNGL/shader_loader.hpp>

#include <cstdint>
#include <exception>
#include <iostream>

int main()
{
    try
    {
        constexpr std::uint32_t width = 512;
        constexpr std::uint32_t height = 512;
        mngl::render_window window("Compute demo", {width, height});
        auto& device = mngl::gpu::device::global();

        auto shader = mngl::gpu::shader_loader::compile_compute("shaders/fill.hlsl");
        auto pipeline = mngl::gpu::compute_pipeline_builder{}
                            .set_compute_shader(shader)
                            .build(device);

        mngl::gpu::frame_graph graph(device);
        auto output = graph.create_texture(
            {width, height}, mngl::gpu::texture_format::r8g8b8a8_unorm,
            mngl::texture::usage::sampler | mngl::texture::usage::compute_write);

        auto fill = graph.add_pass(
            "fill", [&](mngl::gpu::frame_graph::pass_context& pass)
            {
                shader["output"] = pass.texture(output);
                shader["size"] = glm::uvec2{width, height};
                shader["color"] = glm::vec4{0.1f, 0.4f, 0.9f, 1.0f};

                mngl::gpu::compute_pass compute(pass.commands(), pipeline, shader);
                compute.dispatch_for({width, height, 1});
            });
        fill.write(output);

        while (mngl::input::next_frame())
        {
            graph.execute();

            auto frame = window.frame();
            auto result = mngl::sprite::borrowed(graph.texture(output));
            frame.draw(result);
            frame.display();
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

`read()`, `write()` и `read_write()` описывают использование одного ресурса в проходе. Граф добавляет зависимости для read-after-write, write-after-read и write-after-write; для независимых проходов можно дополнительно указать `pass.depends_on(other.handle())`. Объявляйте `read_write()` для ресурсов, которые проход действительно читает и изменяет; transient-текстуры и буферы сначала нужно полностью записать.

## Как граф решает, что запускать первым

Объявление доступа — это контракт для графа и документация для вас. Когда один проход записывает ресурс, а следующий читает его, граф автоматически ставит запись раньше чтения. Такой же порядок задаётся между читателями и последующей записью, чтобы запись не перетёрла данные до чтения. Если порядок важен по другой причине и проходы не делят ресурс, добавьте явный `depends_on()`.

| Объявление | Значение |
|---|---|
| `read(resource)` | Проход читает уже существующее содержимое и не меняет его. |
| `write(resource)` | Проход полностью создаёт содержимое. Подходит для первой записи transient-ресурса. |
| `read_write(resource)` | Проход читает и изменяет прежнее содержимое. Для transient-ресурса перед ним уже должна быть запись. |

Минимальный пример связи двух операций:

```cpp
auto source_handle = graph.import_texture(source); // source создана заранее и принадлежит приложению.
auto intermediate = graph.create_texture(
    {width, height}, mngl::gpu::texture_format::r8g8b8a8_unorm,
    mngl::texture::usage::sampler | mngl::texture::usage::compute_write);
auto result = graph.create_texture(
    {width, height}, mngl::gpu::texture_format::r8g8b8a8_unorm,
    mngl::texture::usage::sampler | mngl::texture::usage::compute_write);

auto first = graph.add_pass("prepare", [&](auto& pass)
{
    // Bind source_handle for reading and intermediate for writing.
    // Record a compute_pass using pass.commands().
});
first.read(source_handle).write(intermediate);

auto second = graph.add_pass("filter", [&](auto& pass)
{
    // Bind intermediate for reading and result for writing.
    // Record another compute_pass using pass.commands().
});
second.read(intermediate).write(result);
```

В примере пропущены shader-specific bindings, но объявления ресурсов уже задают зависимость `prepare → filter`. Если убрать `.write(intermediate)` или `.read(intermediate)`, граф не сможет вывести этот порядок. Во время callback используйте `pass.texture(handle)`, `pass.gpu_texture_resource(handle)` или `pass.buffer(handle)`: context проверяет, что проход заранее объявил доступ к этому handle.

Transient-ресурсы принадлежат графу; `import_texture()` и `import_buffer()` только регистрируют внешние объекты. Для импортированного низкоуровневого `mngl::gpu::texture` используйте `pass.gpu_texture_resource(handle)`; `pass.texture(handle)` возвращает высокоуровневую `mngl::texture` и не подходит низкоуровневому импорту. Держите импортированные объекты, pipeline, shader assets и другие захваченные callback-ом данные живыми до `execute()`.

Каждый `add_pass()` получает `pass.commands()` и может записать `compute_pass` или низкоуровневый `render_pass`. Не вызывайте `submit()` или `cancel()` у этого command buffer внутри callback: граф сам закрывает и отправляет его после всех проходов. Указывайте все используемые ресурсы через `read()`, `write()` или `read_write()`, чтобы граф мог упорядочить проходы. После `graph.execute()` можно использовать результаты и в обычном `window.frame()`.

Импортированные `texture` и `buffer` граф не уничтожает — они должны оставаться живыми, пока выполняются проходы. Каждому transient-ресурсу выделяется отдельный GPU-объект; переиспользование памяти между непересекающимися lifetime пока не выполняется.

Выполнение проходов последовательное. Граф задаёт зависимости и lifetime ресурсов, но не запускает проходы параллельно, не объединяет transient-ресурсы в общий memory pool и не управляет кадром окна. Если callback выбросил исключение, `execute()` отменяет ещё не отправленные команды, а этот экземпляр графа больше нельзя повторно выполнять: создайте его заново после исправления причины.

