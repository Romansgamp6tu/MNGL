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
			std::vector<size_t> to_delete;
			size_t _idx = 0;
			for (auto* i : updatables)
			{
				i->update(dt);
				if (!i)
				{
					logger.warning("an updatable was removed during update loop, this may cause undefined behavior");
					logger.debug_warning("automatically removing object ", i, " from updatables list");
					to_delete.push_back(_idx);
				}
				_idx++;
			}
			if (!to_delete.empty())
			{
				size_t diff = 0;
				for (size_t i = 0; i < to_delete.size(); i++)
				{
					updatables.erase(updatables.begin() + to_delete[i] - diff++);
				}
			}
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