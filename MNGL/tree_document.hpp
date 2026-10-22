#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace tree_practicum
{
	// Editor view state belongs to a document, not to the editor window.
	struct tree_document_view_state
	{
		double zoom = 1.0;
		double pan_x = 0.0;
		double pan_y = 0.0;
		bool initialized = false;
		bool fit_requested = true;
	};

	struct tree_document
	{
		// Path of this document's .tree entry inside the current workspace.
		std::string workspace_path;
		std::filesystem::path local_path;
		std::string display_name;
		tree_document_view_state view;
		std::uint64_t current_state_id = 0;
		std::uint64_t saved_state_id = 0;

		[[nodiscard]] bool is_dirty() const noexcept
		{
			return current_state_id != saved_state_id;
		}
	};
}
