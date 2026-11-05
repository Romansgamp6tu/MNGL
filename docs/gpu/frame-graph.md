# Граф кадра

Обычному 2D-коду граф не нужен: используйте `window.frame()` и `frame.draw(...)`. `mngl::gpu::frame_graph` — отдельный продвинутый слой для сценариев, где compute- и render-проходы обмениваются GPU-ресурсами.

Граф создаёт временные текстуры и буферы и владеет ими до своего уничтожения. Проходы объявляют чтение и запись; по этим объявлениям строится порядок выполнения, а transient-ресурс нельзя прочитать до первого прохода-записи. Внешние ресурсы подключаются через `import_texture()` и `import_buffer()` и остаются во владении вызывающего кода. Все GPU-проходы записываются в один command buffer, который граф отправляет после завершения проходов.

## Compute, затем обычная 2D-отрисовка

Этот пример записывает compute-команду в transient-текстуру, а затем рисует её обычным 2D renderer-ом в окно. Оконный кадр намеренно остаётся вне графа:

```cpp
#include "compute_pass.hpp"
#include "frame_graph.hpp"
#include "render_window.hpp"
#include "renderer2d.hpp"
#include "shader_loader.hpp"

constexpr uint32_t width = 512;
constexpr uint32_t height = 512;
auto& device = mngl::gpu::device::global();
mngl::render_window window("Compute demo", {width, height});

auto shader = mngl::gpu::shader_loader::compile_compute("shaders/fill.hlsl");
auto pipeline = mngl::gpu::compute_pipeline_builder{}.set_compute_shader(shader).build(device);

mngl::gpu::frame_graph graph(device);
auto output = graph.create_texture({width, height}, mngl::gpu::texture_format::r8g8b8a8_unorm,
                                   mngl::texture::usage::sampler | mngl::texture::usage::compute_write);

auto fill = graph.add_pass("fill",
                           [&](mngl::gpu::frame_graph::pass_context& pass)
                           {
	                           shader["output"] = pass.texture(output);
	                           shader["size"] = glm::uvec2{width, height};
	                           shader["color"] = glm::vec4{0.1f, 0.4f, 0.9f, 1.0f};

	                           mngl::gpu::compute_pass compute(pass.commands(), pipeline, shader);
	                           compute.dispatch_for({width, height, 1});
                           });
fill.write(output);

graph.execute();

auto frame = window.frame();
mngl::sprite result(graph.texture(output));
frame.draw(result);
frame.display();
```

`read()`, `write()` и `read_write()` описывают использование одного ресурса в проходе. Граф добавляет зависимости для read-after-write, write-after-read и write-after-write; для независимых проходов можно дополнительно указать `pass.depends_on(other.handle())`. Объявляйте `read_write()` для ресурсов, которые проход действительно читает и изменяет; transient-текстуры и буферы сначала нужно полностью записать.

Каждый `add_pass()` получает `pass.commands()` и может записать `compute_pass` или низкоуровневый `render_pass`. Указывайте все используемые ресурсы через `read()`, `write()` или `read_write()`, чтобы граф мог упорядочить проходы. После `graph.execute()` можно использовать результаты и в обычном `window.frame()`.

Импортированные `texture` и `buffer` граф не уничтожает — они должны оставаться живыми, пока выполняются проходы. Каждому transient-ресурсу выделяется отдельный GPU-объект; переиспользование памяти между непересекающимися lifetime пока не выполняется.

Выполнение проходов последовательное. Граф задаёт зависимости и lifetime ресурсов, но не запускает проходы параллельно и не управляет кадром окна.
