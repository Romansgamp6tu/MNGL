#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace mngl
{
	// не использовать для сложных иерархий, это класс для простых обновлений по типу глобальных анимаций и
	// другой шелухи
	class updatable
	{
	public:
		updatable();
		updatable(const updatable&) = delete;
		updatable& operator=(const updatable&) = delete;
		updatable(updatable&&) = delete;
		updatable& operator=(updatable&&) = delete;
		virtual ~updatable();
		virtual void update(double dt) = 0;
	};

	class update_manager
	{
	public:
		update_manager() = delete;

		static void tick(double dt)
		{
			for (std::size_t i = 0; i < updatables.size(); ++i)
			{
				if (updatables[i])
				{
					updatables[i]->update(dt);
				}
			}

			// В конце кадра в один проход вычищаем все nullptr, которые образовались
			updatables.erase(std::remove(updatables.begin(), updatables.end(), nullptr), updatables.end());
		}

	private:
		static inline std::vector<updatable*> updatables;
		friend class updatable;

		static void add(updatable* i) { updatables.push_back(i); }

		static void remove(updatable* i)
		{
			auto it = std::find(updatables.begin(), updatables.end(), i);
			if (it != updatables.end())
				*it = nullptr;
		}
	};

	inline updatable::~updatable()
	{
		update_manager::remove(this);
	}

	inline updatable::updatable()
	{
		update_manager::add(this);
	}
} // namespace mngl
// Compatibility names; new code uses snake_case.
namespace mngl
{
	using Iupdatable = updatable;
}
