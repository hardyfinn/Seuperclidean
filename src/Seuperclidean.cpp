// Seuperclidean.cpp
// Six Euclidean rhythms with Bias warp, mutation, density/fills, swing,
// humanise, off-beat/accent outputs and three logic outputs.
// The rhythm maths is in EuclidEngine.hpp; this file connects it to knobs,
// jacks, lights and the step display. Positions are in mm and must match
// res/make_panel.py.
#include "plugin.hpp"
#include "EuclidEngine.hpp"

using ew::NUM_CH;
using ew::NUM_LOGIC;
using ew::MAX_LEN;

// ---- layout (mm), shared with res/make_panel.py ----
static const float ROW_Y0 = 48.f, ROW_DY = 12.5f;
static const float X_LEN = 12.f, X_FILL = 23.f, X_ROT = 34.f, X_BIAS = 45.f;
static const float X_SWG = 55.5f, X_HUM = 65.f, X_MUT = 74.5f;
static const float X_CFILL = 85.f, X_CROT = 95.f, X_CBIAS = 105.f;
static const float X_GATE = 116.f, X_OFF = 126.f, X_ACC = 136.f;
static const float RA = 155.f, RB = 169.f, RC = 183.f;
static const float DISP_X = 5.f, DISP_Y = 13.5f, DISP_W = 137.f, DISP_H = 21.f;
static const float LOGIC_Y[NUM_LOGIC] = {84.6f, 98.6f, 112.6f};

struct Seuperclidean : Module {
	enum ParamId {
		ENUMS(LEN_PARAM, NUM_CH),
		ENUMS(FILL_PARAM, NUM_CH),
		ENUMS(ROT_PARAM, NUM_CH),
		ENUMS(BIAS_PARAM, NUM_CH),
		ENUMS(SWING_PARAM, NUM_CH),
		ENUMS(HUMAN_PARAM, NUM_CH),
		ENUMS(MUT_PARAM, NUM_CH),
		DENSITY_PARAM,
		MUTATE_PARAM,
		FILLAMT_PARAM,
		FILL_BUTTON,
		LOCK_BUTTON,
		HOME_BUTTON,
		ENUMS(LOGIC_BUTTON, NUM_LOGIC),
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		FILL_INPUT,
		DENSITY_INPUT,
		MUTATE_INPUT,
		LOCK_INPUT,
		ENUMS(FILL_CV_INPUT, NUM_CH),
		ENUMS(ROT_CV_INPUT, NUM_CH),
		ENUMS(BIAS_CV_INPUT, NUM_CH),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(GATE_OUTPUT, NUM_CH),
		ENUMS(OFF_OUTPUT, NUM_CH),
		ENUMS(ACC_OUTPUT, NUM_CH),
		ENUMS(LOGIC_OUTPUT, NUM_LOGIC),
		OUTPUTS_LEN
	};
	enum LightId {
		FILL_LIGHT,
		LOCK_LIGHT,
		HOME_LIGHT,
		ENUMS(LOGIC_LIGHT, NUM_LOGIC * 4),
		LIGHTS_LEN
	};

	ew::Engine engine;
	dsp::SchmittTrigger clockTrigger, resetTrigger;
	dsp::BooleanTrigger lockButton, homeButton, logicButton[NUM_LOGIC];
	dsp::ClockDivider settingsDivider, lightDivider;
	dsp::PulseGenerator homePulse;
	bool lockLatched = false;

	Seuperclidean() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// starting patterns: kick, clap, off-beat hats, perc, and two odd-length loops
		const int defLen[NUM_CH] = {16, 16, 16, 16, 13, 7};
		const int defFill[NUM_CH] = {4, 2, 4, 5, 5, 3};
		const int defRot[NUM_CH] = {0, 4, 2, 0, 0, 0};
		for (int i = 0; i < NUM_CH; i++) {
			std::string c = string::f("Ch %d ", i + 1);
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
		configParam(DENSITY_PARAM, -1.f, 1.f, 0.f, "Density (adds or removes hits on every channel)", "%", 0.f, 100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 1.f, "Mutate (scales every channel's mutation)", "%", 0.f, 100.f);
		configParam(FILLAMT_PARAM, 0.f, 1.f, 0.5f, "Fill amount", "%", 0.f, 100.f);
		configButton(FILL_BUTTON, "Fill (hold)");
		configButton(LOCK_BUTTON, "Lock mutation (toggle)");
		configButton(HOME_BUTTON, "Home: undo all mutation");
		const char* pairs[NUM_LOGIC] = {"1+2", "3+4", "5+6"};
		for (int i = 0; i < NUM_LOGIC; i++) {
			configButton(LOGIC_BUTTON + i, string::f("Logic %s mode (AND / OR / XOR / A NOT B)", pairs[i]));
			configOutput(LOGIC_OUTPUT + i, string::f("Logic %s", pairs[i]));
		}
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(FILL_INPUT, "Fill gate");
		configInput(DENSITY_INPUT, "Density CV (+-5 V = full range)");
		configInput(MUTATE_INPUT, "Mutate CV (10 V = full range)");
		configInput(LOCK_INPUT, "Lock gate (high = mutation frozen)");
		settingsDivider.setDivision(32);
		lightDivider.setDivision(256);
		engine.rng.setSeed(random::u64());
		updateSettings();
		engine.updatePatterns();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		lockLatched = false;
		engine.logicMode[0] = ew::AND;
		engine.logicMode[1] = ew::XOR;
		engine.logicMode[2] = ew::OR;
		engine.reset();
		engine.home();
	}

	void updateSettings() {
		for (int i = 0; i < NUM_CH; i++) {
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
		engine.density = clamp(params[DENSITY_PARAM].getValue() + inputs[DENSITY_INPUT].getVoltage() / 5.f, -1.f, 1.f);
		engine.mutateAmt = clamp(params[MUTATE_PARAM].getValue() + inputs[MUTATE_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		engine.fillAmt = params[FILLAMT_PARAM].getValue();
		engine.fillActive = params[FILL_BUTTON].getValue() > 0.f || inputs[FILL_INPUT].getVoltage() >= 1.f;

		if (lockButton.process(params[LOCK_BUTTON].getValue() > 0.f))
			lockLatched = !lockLatched;
		engine.locked = lockLatched || inputs[LOCK_INPUT].getVoltage() >= 1.f;
		if (homeButton.process(params[HOME_BUTTON].getValue() > 0.f)) {
			engine.home();
			homePulse.trigger(0.2f);
		}
		for (int i = 0; i < NUM_LOGIC; i++)
			if (logicButton[i].process(params[LOGIC_BUTTON + i].getValue() > 0.f))
				engine.logicMode[i] = (engine.logicMode[i] + 1) % ew::NUM_LOGIC_MODES;
		// keep the display live between clocks
		engine.updatePatterns();
	}

	void process(const ProcessArgs& args) override {
		if (settingsDivider.process())
			updateSettings();

		bool clockEdge = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		bool resetEdge = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		engine.process(args.sampleTime, clockEdge, resetEdge);

		for (int i = 0; i < NUM_CH; i++) {
			outputs[GATE_OUTPUT + i].setVoltage(engine.ch[i].gate.high ? 10.f : 0.f);
			outputs[OFF_OUTPUT + i].setVoltage(engine.ch[i].off.high ? 10.f : 0.f);
			outputs[ACC_OUTPUT + i].setVoltage(engine.ch[i].acc.high ? 10.f : 0.f);
		}
		for (int i = 0; i < NUM_LOGIC; i++)
			outputs[LOGIC_OUTPUT + i].setVoltage(engine.logic[i].high ? 10.f : 0.f);

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[FILL_LIGHT].setBrightness(engine.fillActive ? 1.f : 0.f);
			lights[LOCK_LIGHT].setBrightness(engine.locked ? 1.f : 0.f);
			lights[HOME_LIGHT].setBrightness(homePulse.process(dt) ? 1.f : 0.f);
			for (int i = 0; i < NUM_LOGIC; i++)
				for (int m = 0; m < 4; m++)
					lights[LOGIC_LIGHT + i * 4 + m].setBrightness(engine.logicMode[i] == m ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "gateMode", json_integer(engine.gateMode));
		json_object_set_new(root, "lock", json_boolean(lockLatched));
		json_t* logic = json_array();
		for (int i = 0; i < NUM_LOGIC; i++) json_array_append_new(logic, json_integer(engine.logicMode[i]));
		json_object_set_new(root, "logic", logic);
		// remember where the mutations have wandered to
		json_t* mut = json_array();
		for (int i = 0; i < NUM_CH; i++) {
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
		if (json_t* logic = json_object_get(root, "logic"))
			for (int i = 0; i < NUM_LOGIC && i < (int) json_array_size(logic); i++)
				engine.logicMode[i] = clamp((int) json_integer_value(json_array_get(logic, i)), 0, 3);
		if (json_t* mut = json_object_get(root, "mutation"))
			for (int i = 0; i < NUM_CH && i < (int) json_array_size(mut); i++) {
				json_t* m = json_array_get(mut, i);
				ew::Channel& k = engine.ch[i];
				k.dFill = json_integer_value(json_object_get(m, "fill"));
				k.dRot = json_integer_value(json_object_get(m, "rot"));
				k.dBias = json_number_value(json_object_get(m, "bias"));
				k.flips = (uint32_t) json_integer_value(json_object_get(m, "flips"));
			}
	}
};

// ---- step display: one row per channel, up to 32 steps ----
struct StepDisplay : Widget {
	Seuperclidean* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, mm2px(1.2f));
		nvgFillColor(args.vg, nvgRGB(0x1A, 0x1A, 0x20));
		nvgFill(args.vg);
		Widget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) drawSteps(args);
		Widget::drawLayer(args, layer);
	}

	void drawSteps(const DrawArgs& args) {
		const float s = box.size.x / DISP_W; // px per mm
		const float left = 5.f, pitch = (DISP_W - left - 1.5f) / MAX_LEN;
		const float rowPitch = 3.6f, top = 1.0f;
		const float cw = pitch * 0.8f, chh = rowPitch * 0.75f;
		const NVGcolor HIT = nvgRGB(0xF0, 0x7A, 0x3E);   // playing hit
		const NVGcolor MUT = nvgRGB(0x4C, 0xD6, 0xE0);   // hit added by mutation
		const NVGcolor GONE = nvgRGB(0x6A, 0x4A, 0x3A);  // hit removed by mutation
		const NVGcolor EMPTY = nvgRGB(0x33, 0x33, 0x3C);
		for (int c = 0; c < NUM_CH; c++) {
			int len = 16, step = -1;
			uint32_t mask = 0, home = 0;
			if (module) {
				const ew::Channel& k = module->engine.ch[c];
				len = clamp(k.len, 1, MAX_LEN);
				step = k.step;
				mask = k.mask;
				home = k.homeMask;
			}
			else {
				// browser preview
				const int L[6] = {16, 16, 16, 16, 13, 7}, F[6] = {4, 2, 4, 5, 5, 3}, R[6] = {0, 4, 2, 0, 0, 0};
				len = L[c];
				mask = home = ew::rotate(ew::warpedPattern(L[c], F[c], 0.f), L[c], R[c]);
			}
			float y = (top + c * rowPitch) * s;
			// channel number
			std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
			if (font && font->handle >= 0) nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 2.6f * s);
			nvgFillColor(args.vg, nvgRGB(0x9A, 0x94, 0x88));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(args.vg, 1.4f * s, y, string::f("%d", c + 1).c_str(), NULL);
			for (int i = 0; i < len; i++) {
				bool on = (mask >> i) & 1u, was = (home >> i) & 1u;
				float x = (left + i * pitch) * s;
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x, y, cw * s, chh * s);
				nvgFillColor(args.vg, on ? (was ? HIT : MUT) : (was ? GONE : EMPTY));
				nvgFill(args.vg);
				if (i == step) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x - 0.25f * s, y - 0.25f * s, (cw + 0.5f) * s, (chh + 0.5f) * s);
					nvgStrokeColor(args.vg, nvgRGB(0xFF, 0xFF, 0xFF));
					nvgStrokeWidth(args.vg, 0.35f * s);
					nvgStroke(args.vg);
				}
			}
		}
	}
};

struct SeuperclideanWidget : ModuleWidget {
	SeuperclideanWidget(Seuperclidean* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Seuperclidean.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		StepDisplay* disp = createWidget<StepDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		disp->box.size = mm2px(Vec(DISP_W, DISP_H));
		disp->module = module;
		addChild(disp);

		using S = Seuperclidean;
		for (int i = 0; i < NUM_CH; i++) {
			float y = ROW_Y0 + i * ROW_DY;
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(X_LEN, y)), module, S::LEN_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(X_FILL, y)), module, S::FILL_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(X_ROT, y)), module, S::ROT_PARAM + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(X_BIAS, y)), module, S::BIAS_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(X_SWG, y)), module, S::SWING_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(X_HUM, y)), module, S::HUMAN_PARAM + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(X_MUT, y)), module, S::MUT_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_CFILL, y)), module, S::FILL_CV_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_CROT, y)), module, S::ROT_CV_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_CBIAS, y)), module, S::BIAS_CV_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_GATE, y)), module, S::GATE_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_OFF, y)), module, S::OFF_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_ACC, y)), module, S::ACC_OUTPUT + i));
		}

		// right-hand column
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RA, 20.f)), module, S::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RB, 20.f)), module, S::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RC, 20.f)), module, S::FILL_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RA, 35.f)), module, S::DENSITY_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RB, 35.f)), module, S::DENSITY_INPUT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(RC, 35.f)), module, S::FILL_BUTTON, S::FILL_LIGHT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(RA, 50.f)), module, S::MUTATE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RB, 50.f)), module, S::MUTATE_INPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(RC, 50.f)), module, S::FILLAMT_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(RA, 64.f)), module, S::LOCK_BUTTON, S::LOCK_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RB, 64.f)), module, S::LOCK_INPUT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(RC, 64.f)), module, S::HOME_BUTTON, S::HOME_LIGHT));

		for (int i = 0; i < NUM_LOGIC; i++) {
			float y = LOGIC_Y[i];
			addParam(createParamCentered<TL1105>(mm2px(Vec(RA - 1.f, y + 1.5f)), module, S::LOGIC_BUTTON + i));
			for (int m = 0; m < 4; m++)
				addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(RA + 7.f, y - 4.4f + m * 2.9f)), module, S::LOGIC_LIGHT + i * 4 + m));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(RC + 2.f, y)), module, S::LOGIC_OUTPUT + i));
		}
	}

	void appendContextMenu(Menu* menu) override {
		Seuperclidean* module = getModule<Seuperclidean>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Output shape",
			{"Trigger (5 ms)", "Gate (half a step)"},
			&module->engine.gateMode));
		menu->addChild(createMenuItem("Home: undo all mutation", "",
			[=]() { module->engine.home(); }));
	}
};

Model* modelSeuperclidean = createModel<Seuperclidean, SeuperclideanWidget>("Seuperclidean");
