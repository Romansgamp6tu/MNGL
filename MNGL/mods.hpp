#pragma once

#include "detail/mods/declarations.hpp"

#define MNGL_MODS_DETAIL_JOIN_IMPL(a, b) a##b
#define MNGL_MODS_DETAIL_JOIN(a, b) MNGL_MODS_DETAIL_JOIN_IMPL(a, b)
#define MNGL_MODS_DETAIL_DECLARE_IMPL(n, group_id, public_id, identity, callback)                            \
	namespace                                                                                                \
	{                                                                                                        \
		[[maybe_unused]] const bool MNGL_MODS_DETAIL_JOIN(mngl_mod_declaration_, n) =                        \
		    ::mngl::mods::detail::declaration_catalog::instance().add((group_id), (public_id), (identity),   \
			                                                          (callback));                           \
	}
#define MNGL_MODS_DETAIL_DECLARE(group_id, public_id, identity, callback)                                    \
	MNGL_MODS_DETAIL_DECLARE_IMPL(__COUNTER__, group_id, public_id, identity, callback)

// These macros are intended for host declarations in a .cpp file.
#define REGISTER_MODDABLE_CLASS(Type, ClassId)                                                               \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("0:class:") + (ClassId), (ClassId), std::string("class|") + #Type + "|" + #ClassId,      \
	    [](::mngl::mods::registry& target) { (void)target.define_class<Type>(ClassId); })

#define REGISTER_MODDABLE_FUNCTION(Type, ExtensionId, Fallback)                                              \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("2:extension:") + (ExtensionId), (ExtensionId),                                          \
	    std::string("function|") + #Type + "|" + #ExtensionId + "|" + #Fallback,                             \
	    [](::mngl::mods::registry& target)                                                                   \
	    {                                                                                                    \
		    static_assert(std::derived_from<Type, ::mngl::mods::modable>,                                    \
			              "REGISTER_MODDABLE_FUNCTION owner must derive from mngl::mods::modable");          \
		    ::mngl::mods::detail::declare_function(target, (ExtensionId), (Fallback));                       \
	    })

#define REGISTER_MODDABLE_FUNCTION_AS(Type, Signature, ExtensionId, Fallback)                                \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("2:extension:") + (ExtensionId), (ExtensionId),                                          \
	    std::string("function|") + #Type + "|" + #Signature + "|" + #ExtensionId + "|" + #Fallback,          \
	    [](::mngl::mods::registry& target)                                                                   \
	    {                                                                                                    \
		    static_assert(std::derived_from<Type, ::mngl::mods::modable>,                                    \
			              "REGISTER_MODDABLE_FUNCTION_AS owner must derive from mngl::mods::modable");       \
		    target.define_function<Signature>((ExtensionId), (Fallback));                                    \
	    })

#define REGISTER_MODDABLE_PROPERTY(Type, ValueType, ExtensionId, Scope, DefaultValue, Getter, Setter)        \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("2:extension:") + (ExtensionId), (ExtensionId),                                          \
	    std::string("property|") + #Type + "|" + #ValueType + "|" + #ExtensionId + "|" + #Scope + "|" +      \
	        #DefaultValue + "|" + #Getter + "|" + #Setter,                                                   \
	    [](::mngl::mods::registry& target)                                                                   \
	    {                                                                                                    \
		    static_assert(std::derived_from<Type, ::mngl::mods::modable>,                                    \
			              "REGISTER_MODDABLE_PROPERTY owner must derive from mngl::mods::modable");          \
		    target.define_property<ValueType>((ExtensionId), (Scope), (DefaultValue), (Getter), (Setter));   \
	    })

#define REGISTER_MODDABLE_VALIDATED_PROPERTY(Type, ValueType, ExtensionId, Scope, DefaultValue, Getter,      \
                                             Setter, Validator)                                              \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("2:extension:") + (ExtensionId), (ExtensionId),                                          \
	    std::string("property|") + #Type + "|" + #ValueType + "|" + #ExtensionId + "|" + #Scope + "|" +      \
	        #DefaultValue + "|" + #Getter + "|" + #Setter + "|" + #Validator,                                \
	    [](::mngl::mods::registry& target)                                                                   \
	    {                                                                                                    \
		    static_assert(                                                                                   \
		        std::derived_from<Type, ::mngl::mods::modable>,                                              \
		        "REGISTER_MODDABLE_VALIDATED_PROPERTY owner must derive from mngl::mods::modable");          \
		    target.define_property<ValueType>((ExtensionId), (Scope), (DefaultValue), (Getter), (Setter),    \
			                                  (Validator));                                                  \
	    })

#define REGISTER_MODDABLE_VALUE_PROPERTY(Type, ValueType, ExtensionId, Scope, DefaultValue)                  \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("2:extension:") + (ExtensionId), (ExtensionId),                                          \
	    std::string("property|") + #Type + "|" + #ValueType + "|" + #ExtensionId + "|" + #Scope + "|" +      \
	        #DefaultValue,                                                                                   \
	    [](::mngl::mods::registry& target)                                                                   \
	    {                                                                                                    \
		    static_assert(std::derived_from<Type, ::mngl::mods::modable>,                                    \
			              "REGISTER_MODDABLE_VALUE_PROPERTY owner must derive from mngl::mods::modable");    \
		    target.define_property<ValueType>((ExtensionId), (Scope), (DefaultValue));                       \
	    })

#define REGISTER_MODDABLE_FACTORY(Base, ClassId, ImplementationId, Factory)                                  \
	MNGL_MODS_DETAIL_DECLARE(                                                                                \
	    std::string("1:factory:") + (ClassId) + ":" + (ImplementationId), (ImplementationId),                \
	    std::string("factory|") + #Base + "|" + #ClassId + "|" + #ImplementationId + "|" + #Factory,         \
	    [](::mngl::mods::registry& target)                                                                   \
	    { target.classes<Base>(ClassId).add_builtin((ImplementationId), (Factory)); })
