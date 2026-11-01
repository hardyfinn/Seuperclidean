#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelSeuperclidean);
	p->addModel(modelSeuperclidean4);
}
