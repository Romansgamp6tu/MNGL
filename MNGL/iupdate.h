#pragma once

#include <vector>
#include "utilities.hpp"

namespace mngl
{
	// не использовать для сложных иерархий, это класс для простых обновлений по типу глобальных анимаций и другой шелухи
	class Iupdatable
	{
	public:
		Iupdatable();
		virtual ~Iupdatable();
		virtual void update(double dt) = 0;
	};

	class update_manager
	{
	public:
		update_manager() = delete;

		static void tick(double dt)
		{
			for (size_t i = 0; i < updatables.size(); ++i)
			{
				if (updatables[i])
				{
					updatables[i]->update(dt);
				}
			}

			// В конце кадра в один проход вычищаем все nullptr, которые образовались
			updatables.erase(
				std::remove(updatables.begin(), updatables.end(), nullptr),
				updatables.end()
			);
		}
	private:
		static inline std::vector<Iupdatable*> updatables;
		friend class Iupdatable;

		static void add(Iupdatable* i)
		{
			updatables.push_back(i);
		}

		static void remove(Iupdatable* i)
		{
			auto it = std::find(updatables.begin(), updatables.end(), i);
			if (it != updatables.end())
				*it = nullptr;
		}
	};

	Iupdatable::~Iupdatable()
	{
		update_manager::remove(this);
	}

	Iupdatable::Iupdatable()
	{
		update_manager::add(this);
	}
}