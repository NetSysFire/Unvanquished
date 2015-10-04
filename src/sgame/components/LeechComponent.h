#ifndef LEECH_COMPONENT_H_
#define LEECH_COMPONENT_H_

#include "../backend/CBSEBackend.h"
#include "../backend/CBSEComponents.h"

class LeechComponent: public LeechComponentBase {
	public:
		// ///////////////////// //
		// Autogenerated Members //
		// ///////////////////// //

		/**
		 * @brief Default constructor of the LeechComponent.
		 * @param entity The entity that owns the component instance.
		 * @param r_AlienBuildableComponent A AlienBuildableComponent instance that this component depends on.
		 * @param r_MiningComponent A MiningComponent instance that this component depends on.
		 * @note This method is an interface for autogenerated code, do not modify its signature.
		 */
		LeechComponent(Entity& entity, AlienBuildableComponent& r_AlienBuildableComponent, MiningComponent& r_MiningComponent);

		// ///////////////////// //

	private:

};

#endif // LEECH_COMPONENT_H_