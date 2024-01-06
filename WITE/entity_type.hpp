#include "Callback.hpp"

namespace WITE::DB {

  struct entity_type {
    DBRecord::type_t typeId;

    typedefCB(update_t, void, DBRecord*, DBEntity*)
    const update_t_ce onUpdate;//happens once per frame

    typedefCB(meta_t, void, DBEntity*);

    //once per object, not re-called when the game loads. For setting up the db record's initial state
    const meta_t_ce onAllocate;

    //At most once per object, not called when the game closes, for freeing any subsidiary db records
    const meta_t_ce onDeallocate;

    //once per object per session, called after onAllocate when newly created, and again when the game loads, for setting up transient resources. Not allowed to access the db (because it might not be fully loaded yet)
    const meta_t_ce onSpinUp;

    //once per object per session, called before onDeallocate when destroying the object, or when the game closes, for freeing transient resources. Should not write to the db (use deallocate for that)
    const meta_t_ce onSpinDown;

    constexpr entity_type(const DBRecord::type_t type,
			  const update_t_ce onUpdate,
			  const meta_t_ce onAllocate,
			  const meta_t_ce onDeallocate,
			  const meta_t_ce onSpinUp,
			  const meta_t_ce onSpinDown) :
      typeId(type),
      onUpdate(onUpdate),
      onAllocate(onAllocate),
      onDeallocate(onDeallocate),
      onSpinUp(onSpinUp),
      onSpinDown(onSpinDown) {}

    constexpr entity_type() {
      typeId = ~0;
    };

    constexpr entity_type(const DBRecord::type_t type) {
      typeId = type;
    };

    template<class T> static constexpr entity_type makeFrom(const DBRecord::type_t type);

  };

  MAKE_FUNC_TEST(entity_type::update_t, onUpdate, DBRecord* d COMMA DBEntity* e, d COMMA e);
  MAKE_FUNC_TEST(entity_type::meta_t, onAllocate, DBEntity* e, e);
  MAKE_FUNC_TEST(entity_type::meta_t, onDeallocate, DBEntity* e, e);
  MAKE_FUNC_TEST(entity_type::meta_t, onSpinUp, DBEntity* e, e);
  MAKE_FUNC_TEST(entity_type::meta_t, onSpinDown, DBEntity* e, e);

  template<class T> constexpr entity_type entity_type::makeFrom(const DBRecord::type_t type) {
    return entity_type(type,
		       get_member_onUpdate_or_NULL<T>::value,
		       get_member_onAllocate_or_NULL<T>::value,
		       get_member_onDeallocate_or_NULL<T>::value,
		       get_member_onSpinUp_or_NULL<T>::value,
		       get_member_onSpinDown_or_NULL<T>::value);
  };

}
