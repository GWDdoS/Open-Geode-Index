#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

#define SETTING(type, key_name) Mod::get()->getSettingValue<type>(key_name)

auto enabled = false;

$on_mod(Loaded) {
    web::WebRequestInterceptEvent().listen(
        [](std::string_view id, web::WebRequest& req) {
            log::debug("{}(id {}, req {})", __func__, id, req.getUrl());

            auto self = &req;
			std::string givenUrl = req.getUrl().data();

            if (enabled and string::contains(givenUrl.data(), "api.geode-sdk.org/v1/mods")) {

                if (givenUrl == "https://api.geode-sdk.org/v1/mods") {
                    self->param("status", SETTING(std::string, "status"));
                    if (auto par = SETTING(std::string, "gd"); par.size() > 1) self->param("gd", par);
                    if (auto par = SETTING(std::string, "platforms"); par.size() > 1) self->param("platforms", par);
                    if (auto par = SETTING(std::string, "tags"); par.size() > 1) self->param("tags", par);
                    if (auto par = SETTING(std::string, "geode"); par.size() > 1) self->param("geode", par);
                    if (auto par = SETTING(std::string, "page"); par.size() > 1) self->param("page", par);
                    if (auto par = SETTING(std::string, "per_page"); par.size() > 1) self->param("per_page", par);
                }

                CCNode* version = CCScene::get()->getChildByIDRecursive("version");
                CCLabelBMFont* value_label = typeinfo_cast<CCLabelBMFont*>(
                    version ? version->getChildByIDRecursive("value-label") : nullptr
                );
                if (value_label) givenUrl = string::replace(
                    givenUrl.data(), "latest",
                    string::replace(value_label->getString(), "v", "")
                );

                if (string::contains(givenUrl.data(), "/logo")) {
                    auto spliturl = string::split(givenUrl.data(), "/");
                    givenUrl = fmt::format(
                        "https://geode-comments.bccst.ru/mod.logo.php?id={}",
                        spliturl[spliturl.size() - 2]
                    );
                }

            }

            if (string::contains(givenUrl.data(), "/mods/updates")) givenUrl = string::replace(
                givenUrl.data(), "api.geode-sdk.org/v1/mods/updates",
                "geode-comments.bccst.ru/mod.updates.php"
            );

			self->url(givenUrl);

            return ListenerResult::Propagate; 
        }, Priority::Stub
    ).leak();
}

void TOGGLE_MAIN() {
	// there was a lot of removed code here ~
    enabled = !enabled;
}

#include <Geode/modify/CCMenuItem.hpp>
class $modify(ModListButtons, CCMenuItem) {
    $override void activate() {
        CCMenuItem::activate();
        if (this == nullptr) return;
        if (this->m_pListener == nullptr) return;
        if (!typeinfo_cast<CCNode*>(this->m_pListener)) return;

        Ref<CCNode> listener = typeinfo_cast<CCNode*>(this->m_pListener);

        if (listener->getID() == "ModList") {

            if (this->getID() == "sort-button" or this->getID() == "filters-button") {

                findFirstChildRecursive<FLAlertLayer>(CCScene::get(),
                    [](FLAlertLayer* __this) {

                        findFirstChildRecursive<CCMenuItemSpriteExtra>(
                            __this, [](CCMenuItemSpriteExtra* item) {
                                auto image_cast = typeinfo_cast<ButtonSprite*>(item->getNormalImage());
                                if (!image_cast) return false;
                                if (image_cast->m_caption.c_str() != std::string("OK")) return false;
                                auto& org_pfnSelector = item->m_pfnSelector;
                                auto& org_pListener = item->m_pListener;
                                if (org_pListener and org_pfnSelector) CCMenuItemExt::assignCallback<CCMenuItem>(item,
                                    [org_pfnSelector, org_pListener](CCMenuItem* item) {
                                        (org_pListener->*org_pfnSelector)(item);
                                        if (auto reload_btn = typeinfo_cast<CCMenuItem*>(
                                            CCScene::get()->getChildByIDRecursive("reload-button")
                                        )) reload_btn->activate();
                                    }
                                );
                                return true;
                            }
                        );

                        return true;
                    }
                );

            }

            if (this->getID() == "filters-button") {

                findFirstChildRecursive<FLAlertLayer>(CCScene::get(),
                    [](FLAlertLayer* __this) {

                        //is "Search Filters"
                        /*if (!findFirstChildRecursive<CCLabelBMFont>(
                            __this, [](CCLabelBMFont* lbl) {
                                return lbl->getString() == std::string("Search Filters");
                            })) return false;*/

                        auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6,
                            [](auto) {
                                TOGGLE_MAIN();
                            }
                        );
                        toggle->toggle(enabled);
                        toggle->setPosition(20, 20);

                        __this->m_buttonMenu->addChild(toggle);

                        auto Label = CCLabelBMFont::create("Unverified", "bigFont.fnt");
                        Label->setScale(0.40f);
                        Label->setAnchorPoint({ 0.f, 0.5f });
                        Label->setPosition(32, 20);
                        __this->m_buttonMenu->addChild(Label);

                        return true;
                    }
                );

            }

        };
    }
};


//deps and dodeps
#include <Geode/modify/MenuLayer.hpp>
class $modify(MenuLayerExt, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

        static int ok = 0;
        if (ok++) return ok;

		static auto id = std::string(getMod()->getID());
		static auto repo = getMod()->getMetadata().getLinks().getSourceURL().value_or("https://github.com/lil2kki/Unverified-Mods");

		//size check
		auto webListener = new async::TaskHolder<web::WebResponse>;
		auto req = web::WebRequest();
		req.onProgress([_this = Ref(this), webListener](web::WebProgress const& prog) {
			//log::debug("{}", prog.downloadTotal());

			if (prog.downloadTotal() > 0) void(); else return;

			webListener->cancel();

			auto err_code_to_ignore = std::error_code();
			auto installed_size = std::filesystem::file_size(getMod()->getPackagePath(), err_code_to_ignore);
			auto actual_size = prog.downloadTotal();

			if (installed_size == actual_size) return;

            openInfoPopup(id);

			auto pop = geode::createQuickPopup(
                (getMod()->getName() + " Update!").c_str(),
				fmt::format(
					"Latest release size mismatch with installed one!"
					"\n" "Download latest release of mod?"
				),
				"Later.", "Yes", [](CCNode* pop, bool Yes) {
					if (!Yes) return;

					Ref state_win = Notification::create("Downloading... (///%)");
					state_win->setTime(1337.f);
					state_win->show();

					auto dlReq = web::WebRequest();

					dlReq.onProgress([state_win](web::WebProgress const& p) {
						state_win->setString(fmt::format("Downloading... ({}%)", (int)p.downloadProgress().value_or(000)));
						});

					auto listener = new async::TaskHolder<web::WebResponse>;
					listener->spawn(
						dlReq.get(repo + "/releases/latest/download/" + id + ".geode"),
						[state_win](web::WebResponse res) {
							std::string data = res.string().unwrapOr("no res");
							state_win->removeFromParent();
                            openModsList();
							if (res.code() < 399) {
								log::debug("{}", res.into(getMod()->getPackagePath()).err());
								auto geode = Loader::get()->getInstalledMod("geode.loader");//log-thread
								geode->setSettingValue("log-thread", !geode->getSettingValue<bool>("log-thread"));
								geode->setSettingValue("log-thread", !geode->getSettingValue<bool>("log-thread"));
							}
							else {
								auto asd = geode::createQuickPopup(
									"Request exception",
									data,
									"OK", nullptr, 420.f, nullptr, false
								);
								asd->show();
							};
						}
					);

				}, false
			);
            pop->m_scene = OverlayManager::get();
			pop->show();
			});
		webListener->spawn(req.get(repo + "/releases/latest/download/" + id + ".geode"), [](auto) {});

		return true;
	}
};