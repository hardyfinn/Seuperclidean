// Seuperclidean4.cpp
// Four-voice Seuperclidean: each voice in its own column under a step ring,
// and four logic outputs comparing 1+2, 3+4, 1+3 and 2+4.
// Same engine as the 6-channel module (EuclidEngine.hpp). Positions are in mm
// and must match res/make_panel4.py.
#include "plugin.hpp"
#include "EuclidEngine.hpp"

using ew::MAX_LEN;

namespace s4 {
static const int NV = 4;  // voices
static const int NL = 4;  // logic outputs
static const float COL_X[NV] = {15.5f, 45.5f, 76.f, 106.f};
static const float RING_Y = 26.f, RING_R = 9.5f;
static const float Y_KNOB1 = 46.f, Y_KNOB2 = 58.5f, Y_TRIM = 70.5f, Y_CV = 82.f, Y_OUT = 94.6f;
static const float Y_BOTTOM = 111.6f;
static const float LOGIC_X[NL] = {72.f, 85.f, 98.f, 111.f};
static const float Y_MODE = 110.6f, Y_LOGIC_OUT = 119.f;
static const int PAIR_A[NL] = {0, 2, 0, 1};
static const int PAIR_B[NL] = {1, 3, 2, 3};
static const NVGcolor VOICE_COL[NV] = {
	nvgRGB(0xF0, 0x7A, 0x3E), nvgRGB(0xE8, 0xC5, 0x47), nvgRGB(0x4C, 0xD6, 0xE0), nvgRGB(0xB3, 0x8C, 0xF0)};
} // namespace s4

using namespace s4;

struct Seuperclidean4 : Module {
	enum ParamId {
		ENUMS(LEN_PARAM, NV),
		ENUMS(FILL_PARAM, NV),
		ENUMS(ROT_PARAM, NV),
		ENUMS(BIAS_PARAM, NV),
		ENUMS(SWING_PARAM, NV),
		ENUMS(HUMAN_PARAM, NV),
		ENUMS(MUT_PARAM, NV),
		DENSITY_PARAM,
		MUTATE_PARAM,
		LOCK_BUTTON,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		FILL_INPUT,
		ENUMS(FILL_CV_INPUT, NV),
		ENUMS(ROT_CV_INPUT, NV),
		ENUMS(BIAS_CV_INPUT, NV),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(GATE_OUTPUT, NV),
		ENUMS(OFF_OUTPUT, NV),
		ENUMS(ACC_OUTPUT, NV),
		ENUMS(LOGIC_OUTPUT, NL),
		OUTPUTS_LEN
	};
	enum LightId {
		LOCK_LIGHT,
		LIGHTS_LEN
	};

	ew::Engine engine;
	dsp::SchmittTrigger clockTrigger, resetTrigger;
	dsp::BooleanTrigger lockButton;
	dsp::ClockDivider settingsDivider, lightDivider;
	bool lockLatched = false;
	float fillAmt = 0.5f;

	void defaultLogic() {
		const int modes[NL] = {ew::AND, ew::XOR, ew::XOR, ew::OR};
		for (int i = 0; i < NL; i++) engine.logicMode[i] = modes[i];
	}

	Seuperclidean4() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		engine.numCh = NV;
		engine.numLogic = NL;
		for (int i = 0; i < NL; i++) {
			engine.logicA[i] = PAIR_A[i];
			engine.logicB[i] = PAIR_B[i];
		}
		defaultLogic();
		// starting patterns: kick, clap, off-beat hats, 13-step perc
		const int defLen[NV] = {16, 16, 16, 13};
		const int defFill[NV] = {4, 2, 4, 5};
		const int defRot[NV] = {0, 4, 2, 0};
		for (int i = 0; i < NV; i++) {
			std::string c = string::f("Voice %d ", i + 1);
			configParam(LEN_PARAM + i, 1.f, MAX_LEN, defLen[i], c + "length", " steps")->snapEnabled = true;
			configParam(FILL_PARAM + i, 0.f, MAX_LEN, defFill[i], c + "fill (hits)")->snapEnabled = true;
			configParam(ROT_PARAM + i, 0.f, MAX_LEN - 1, defRot[i], c + "rotation", " steps")->snapEnabled = true;
			configParam(BIAS_PARAM + i, -1.f, 1.f, 0.f, c + "bias (- early, + late)", "%", 0.f, 100.f);
			configParam(SWING_PARAM + i, 0.f, 1.f, 0.f, c + "swing", "%", 0.f, 25.f, 50.f);
			configParam(HUMAN_PARAM + i, 0.f, 1.f, 0.f, c + "humanise (random lateness)", " ms", 0.f, 30.f);
			configParam(MUT_PARAM + i, 0.f, 1.f, 0.f, c + "mutation chance per cycle", "%", 0.f, 100.f);
			configInput(FILL_CV_INPUT + i, c + "fill CV (10 V = +length)");
			configInput(ROT_CV_INPUT + i, c + "rotation CV (10 V = one full turn)");
			configInput(BIAS_CV_INPUT + i, c + "bias CV (+-5 V = full range)");
			configOutput(GATE_OUTPUT + i, c + "gate");
			configOutput(OFF_OUTPUT + i, c + "off-beats (the gaps)");
			configOutput(ACC_OUTPUT + i, c + "accent (the pattern's downbeat)");
		}
		for (int i = 0; i < NL; i++)
			configOutput(LOGIC_OUTPUT + i, string::f("Logic %d+%d (click the mode above to change it)", PAIR_A[i] + 1, PAIR_B[i] + 1));
		configParam(DENSITY_PARAM, -1.f, 1.f, 0.f, "Density (adds or removes hits on every voice)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 1.f, "Mutate (scales every voice's mutation)", "%", 0.f, 100.f);
		configButton(LOCK_BUTTON, "Lock mutation (toggle)");
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(FILL_INPUT, "Fill gate");
		settingsDivider.setDivision(32);
		lightDivider.setDivision(256);
		engine.rng.setSeed(random::u64());
		updateSettings();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		lockLatched = false;
		fillAmt = 0.5f;
		defaultLogic();
		engine.reset();
		engine.home();
	}

	void updateSettings() {
		for (int i = 0; i < NV; i++) {
			ew::Channel& k = engine.ch[i];
			int len = clamp((int) std::round(params[LEN_PARAM + i].getValue()), 1, MAX_LEN);
			k.len = len;
			float fill = params[FILL_PARAM + i].getValue() + inputs[FILL_CV_INPUT + i].getVoltage() / 10.f * len;
			k.fill = clamp((int) std::round(fill), 0, len);
			float rot = params[ROT_PARAM + i].getValue() + inputs[ROT_CV_INPUT + i].getVoltage() / 10.f * len;
			k.rot = (int) std::round(rot);
			k.bias = clamp(params[BIAS_PARAM + i].getValue() + inputs[BIAS_CV_INPUT + i].getVoltage() / 5.f, -1.f, 1.f);
			k.swing = params[SWING_PARAM + i].getValue();
			k.human = params[HUMAN_PARAM + i].getValue();
			k.mutate = params[MUT_PARAM + i].getValue();
		}
		engine.density = params[DENSITY_PARAM].getValue();
		engine.mutateAmt = params[MUTATE_PARAM].getValue();
		engine.fillAmt = fillAmt;
		engine.fillActive = inputs[FILL_INPUT].getVoltage() >= 1.f;
		if (lockButton.process(params[LOCK_BUTTON].getValue() > 0.f))
			lockLatched = !lockLatched;
		engine.locked = lockLatched;
		engine.updatePatterns();
	}

	void process(const ProcessArgs& args) override {
		if (settingsDivider.process())
			updateSettings();

		bool clockEdge = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		bool resetEdge = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		engine.process(args.sampleTime, clockEdge, resetEdge);

		for (int i = 0; i < NV; i++) {
			outputs[GATE_OUTPUT + i].setVoltage(engine.ch[i].gate.high ? 10.f : 0.f);
			outputs[OFF_OUTPUT + i].setVoltage(engine.ch[i].off.high ? 10.f : 0.f);
			outputs[ACC_OUTPUT + i].setVoltage(engine.ch[i].acc.high ? 10.f : 0.f);
		}
		for (int i = 0; i < NL; i++)
			outputs[LOGIC_OUTPUT + i].setVoltage(engine.logic[i].high ? 10.f : 0.f);

		if (lightDivider.process())
			lights[LOCK_LIGHT].setBrightness(engine.locked ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "gateMode", json_integer(engine.gateMode));
		json_object_set_new(root, "lock", json_boolean(lockLatched));
		json_object_set_new(root, "fillAmt", json_real(fillAmt));
		json_t* logic = json_array();
		for (int i = 0; i < NL; i++) json_array_append_new(logic, json_integer(engine.logicMode[i]));
		json_object_set_new(root, "logic", logic);
		json_t* mut = json_array();
		for (int i = 0; i < NV; i++) {
			const ew::Channel& k = engine.ch[i];
			json_t* m = json_object();
			json_object_set_new(m, "fill", json_integer(k.dFill));
			json_object_set_new(m, "rot", json_integer(k.dRot));
			json_object_set_new(m, "bias", json_real(k.dBias));
			json_object_set_new(m, "flips", json_integer(k.flips));
			json_array_append_new(mut, m);
		}
		json_object_set_new(root, "mutation", mut);
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* g = json_object_get(root, "gateMode"))
			engine.gateMode = clamp((int) json_integer_value(g), 0, 1);
		if (json_t* l = json_object_get(root, "lock"))
			lockLatched = json_boolean_value(l);
		if (json_t* f = json_object_get(root, "fillAmt"))
			fillAmt = clamp((float) json_number_value(f), 0.f, 1.f);
		if (json_t* logic = json_object_get(root, "logic"))
			for (int i = 0; i < NL && i < (int) json_array_size(logic); i++)
				engine.logicMode[i] = clamp((int) json_integer_value(json_array_get(logic, i)), 0, 3);
		if (json_t* mut = json_object_get(root, "mutation"))
			for (int i = 0; i < NV && i < (int) json_array_size(mut); i++) {
				json_t* m = json_array_get(mut, i);
				ew::Channel& k = engine.ch[i];
				k.dFill = json_integer_value(json_object_get(m, "fill"));
				k.dRot = json_integer_value(json_object_get(m, "rot"));
				k.dBias = json_number_value(json_object_get(m, "bias"));
				k.flips = (uint32_t) json_integer_value(json_object_get(m, "flips"));
			}
	}
};

static std::shared_ptr<window::Font> displayFont() {
	return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
}

// ---- step ring for one voice ----
struct StepRing : Widget {
	Seuperclidean4* module = nullptr;
	int voice = 0;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, mm2px(1.6f));
		nvgFillColor(args.vg, nvgRGB(0x1A, 0x1A, 0x20));
		nvgFill(args.vg);
		Widget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) drawRing(args);
		Widget::drawLayer(args, layer);
	}

	void drawRing(const DrawArgs& args) {
		int len = 16, fill = 4, step = -1;
		uint32_t mask, home;
		if (module) {
			const ew::Channel& k = module->engine.ch[voice];
			len = clamp(k.len, 1, MAX_LEN);
			step = k.step;
			mask = k.mask;
			home = k.homeMask;
			fill = 0;
			for (int i = 0; i < len; i++) fill += (mask >> i) & 1u;
		}
		else {
			const int L[NV] = {16, 16, 16, 13}, F[NV] = {4, 2, 4, 5}, R[NV] = {0, 4, 2, 0};
			len = L[voice];
			fill = F[voice];
			mask = home = ew::rotate(ew::warpedPattern(L[voice], F[voice], 0.f), L[voice], R[voice]);
		}
		const float cx = box.size.x / 2, cy = box.size.y / 2;
		const float r = mm2px(RING_R);
		float pitch = 2.f * M_PI * RING_R / len;
		float dot = mm2px(std::min(1.15f, 0.42f * pitch));
		const NVGcolor col = VOICE_COL[voice];
		const NVGcolor MUT = nvgRGB(0xFF, 0xFF, 0xFF);
		const NVGcolor GONE = nvgRGB(0x5A, 0x44, 0x3A);
		const NVGcolor EMPTY = nvgRGB(0x3A, 0x3A, 0x44);
		for (int i = 0; i < len; i++) {
			float a = -M_PI / 2 + 2.f * M_PI * i / len;
			float x = cx + r * std::cos(a), y = cy + r * std::sin(a);
			bool on = (mask >> i) & 1u, was = (home >> i) & 1u;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, x, y, dot);
			nvgFillColor(args.vg, on ? (was ? col : MUT) : (was ? GONE : EMPTY));
			nvgFill(args.vg);
			if (i == step) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, x, y, dot + mm2px(0.45f));
				nvgStrokeColor(args.vg, nvgRGB(0xFF, 0xFF, 0xFF));
				nvgStrokeWidth(args.vg, mm2px(0.3f));
				nvgStroke(args.vg);
			}
		}
		std::shared_ptr<window::Font> font = displayFont();
		if (font && font->handle >= 0) nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(0x9A, 0x94, 0x88));
		nvgFontSize(args.vg, mm2px(3.4f));
		nvgText(args.vg, cx, cy - mm2px(1.3f), string::f("%d", voice + 1).c_str(), NULL);
		nvgFontSize(args.vg, mm2px(2.0f));
		nvgText(args.vg, cx, cy + mm2px(2.0f), string::f("%d/%d", fill, len).c_str(), NULL);
	}
};

// ---- logic mode readout: click to change ----
struct LogicModeButton : OpaqueWidget {
	Seuperclidean4* module = nullptr;
	int index = 0;

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			int& m = module->engine.logicMode[index];
			m = (m + 1) % ew::NUM_LOGIC_MODES;
			e.consume(this);
		}
		OpaqueWidget::onButton(e);
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, mm2px(0.8f));
		nvgFillColor(args.vg, nvgRGB(0x14, 0x14, 0x18));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x55, 0x55, 0x60));
		nvgStrokeWidth(args.vg, mm2px(0.2f));
		nvgStroke(args.vg);
		OpaqueWidget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int m = module ? module->engine.logicMode[index] : (index == 0 ? ew::AND : index == 3 ? ew::OR : ew::XOR);
			std::string s;
			switch (m) {
				case ew::AND: s = "AND"; break;
				case ew::OR: s = "OR"; break;
				case ew::XOR: s = "XOR"; break;
				default: s = string::f("ONLY %d", PAIR_A[index] + 1); break;
			}
			std::shared_ptr<window::Font> font = displayFont();
			if (font && font->handle >= 0) nvgFontFaceId(args.vg, font->handle);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFontSize(args.vg, mm2px(2.4f));
			nvgFillColor(args.vg, nvgRGB(0xF0, 0xC0, 0x40));
			nvgText(args.vg, box.size.x / 2, box.size.y / 2, s.c_str(), NULL);
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

struct Seuperclidean4Widget : ModuleWidget {
	Seuperclidean4Widget(Seuperclidean4* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Seuperclidean4.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		using S = Seuperclidean4;
		for (int i = 0; i < NV; i++) {
			float x = COL_X[i];
			float half = RING_R + 2.5f;
			StepRing* ring = createWidget<StepRing>(mm2px(Vec(x - half, RING_Y - half)));
			ring->box.size = mm2px(Vec(2 * half, 2 * half));
			ring->module = module;
			ring->voice = i;
			addChild(ring);

			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x - 6.5f, Y_KNOB1)), module, S::LEN_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x + 6.5f, Y_KNOB1)), module, S::FILL_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x - 6.5f, Y_KNOB2)), module, S::ROT_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x + 6.5f, Y_KNOB2)), module, S::BIAS_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x - 8.5f, Y_TRIM)), module, S::SWING_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, Y_TRIM)), module, S::HUMAN_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x + 8.5f, Y_TRIM)), module, S::MUT_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x - 9.f, Y_CV)), module, S::FILL_CV_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, Y_CV)), module, S::ROT_CV_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x + 9.f, Y_CV)), module, S::BIAS_CV_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x - 9.f, Y_OUT)), module, S::GATE_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, Y_OUT)), module, S::OFF_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x + 9.f, Y_OUT)), module, S::ACC_OUTPUT + i));
		}

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.f, Y_BOTTOM)), module, S::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.f, Y_BOTTOM)), module, S::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.f, Y_BOTTOM)), module, S::FILL_INPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.f, Y_BOTTOM)), module, S::DENSITY_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(49.f, Y_BOTTOM)), module, S::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(58.f, Y_BOTTOM)), module, S::LOCK_BUTTON, S::LOCK_LIGHT));

		for (int i = 0; i < NL; i++) {
			LogicModeButton* b = createWidget<LogicModeButton>(mm2px(Vec(LOGIC_X[i] - 5.5f, Y_MODE - 2.2f)));
			b->box.size = mm2px(Vec(11.f, 4.4f));
			b->module = module;
			b->index = i;
			addChild(b);
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(LOGIC_X[i], Y_LOGIC_OUT)), module, S::LOGIC_OUTPUT + i));
		}
	}

	void appendContextMenu(Menu* menu) override {
		Seuperclidean4* module = getModule<Seuperclidean4>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Output shape",
			{"Trigger (5 ms)", "Gate (half a step)"},
			&module->engine.gateMode));
		menu->addChild(createSubmenuItem("Fill amount", string::f("%d%%", (int) std::round(module->fillAmt * 100.f)),
			[=](Menu* sub) {
				for (int pct : {25, 50, 75, 100})
					sub->addChild(createCheckMenuItem(string::f("%d%%", pct), "",
						[=]() { return (int) std::round(module->fillAmt * 100.f) == pct; },
						[=]() { module->fillAmt = pct / 100.f; }));
			}));
		menu->addChild(createMenuItem("Home: undo all mutation", "",
			[=]() { module->engine.home(); }));
	}
};

Model* modelSeuperclidean4 = createModel<Seuperclidean4, Seuperclidean4Widget>("Seuperclidean4");
