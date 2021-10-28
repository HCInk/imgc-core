#include <torasu/torasu.hpp>

#include <torasu/mod/imgc/imgc_full.hpp>

namespace {

static const torasu::DataResourceFactory* dataFactories[] = {
	// TODO Implement factories for the other types
};
static const torasu::ElementFactory* elementFactories[] = {
	imgc::Rcolor::Rcolor::FACTORY,
	// TODO Implement factories for the other elements
};

static torasu::DiscoveryInterface::FactoryIndex factoryIndex = {
	// Data Factories
	.dataFactoryIndex = dataFactories,
	.dataFactoryCount = sizeof(dataFactories)/sizeof(torasu::DataResourceFactory),
	// Element Factories
	.elementFactoryIndex = elementFactories,
	.elementFactoryCount = sizeof(elementFactories)/sizeof(torasu::ElementFactory),
};

static class : public torasu::DiscoveryInterface {

	const FactoryIndex* getFactoryIndex() const override {
		return &factoryIndex;
	}

	torasu::UserLabel getLabel() const override {
		return {
			.name = "IMGC",
			.description = "Image Processing for TORASU"
		};
	}

} DISCOVERY;

} // namespace

extern "C" const torasu::DiscoveryInterface* TORASU_DISCOVERY_imgc() {
	return &DISCOVERY;
}
