#include "tooltips.hpp"

#include "window_manager.hpp"
#include "widgets.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/world.hpp"
#include "../mwbase/environment.hpp"

#include <boost/lexical_cast.hpp>

using namespace MWGui;
using namespace MyGUI;

ToolTips::ToolTips(WindowManager* windowManager) :
    Layout("openmw_tooltips.xml")
    , mGameMode(true)
    , mWindowManager(windowManager)
    , mFullHelp(false)
    , mEnabled(true)
    , mFocusToolTipX(0.0)
    , mFocusToolTipY(0.0)
{
    getWidget(mDynamicToolTipBox, "DynamicToolTipBox");

    mDynamicToolTipBox->setVisible(false);

    // turn off mouse focus so that getMouseFocusWidget returns the correct widget,
    // even if the mouse is over the tooltip
    mDynamicToolTipBox->setNeedMouseFocus(false);
    mMainWidget->setNeedMouseFocus(false);
}

void ToolTips::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

void ToolTips::onFrame(float frameDuration)
{
    MyGUI::Gui::getInstance().destroyWidget(mDynamicToolTipBox);
    mDynamicToolTipBox = mMainWidget->createWidget<Widget>("HUD_Box",
        IntCoord(0, 0, mMainWidget->getCoord().width, mMainWidget->getCoord().height),
        Align::Stretch, "DynamicToolTipBox");

    // start by hiding everything
    for (unsigned int i=0; i < mMainWidget->getChildCount(); ++i)
    {
        mMainWidget->getChildAt(i)->setVisible(false);
    }

    const IntSize &viewSize = RenderManager::getInstance().getViewSize();

    if (!mEnabled)
    {
        return;
    }

    if (!mGameMode)
    {
        Widget* focus = InputManager::getInstance().getMouseFocusWidget();
        if (focus == 0)
        {
            return;
        }

        IntSize tooltipSize;

        std::string type = focus->getUserString("ToolTipType");
        std::string text = focus->getUserString("ToolTipText");

        ToolTipInfo info;
        if (type == "")
        {
            return;
        }
        else if (type == "ItemPtr")
        {
            mFocusObject = *focus->getUserData<MWWorld::Ptr>();
            tooltipSize = getToolTipViaPtr(false);
        }
        else if (type == "Layout")
        {
            // tooltip defined in the layout
            MyGUI::Widget* tooltip;
            getWidget(tooltip, focus->getUserString("ToolTipLayout"));

            tooltip->setVisible(true);
            if (!tooltip->isUserString("DontResize"))
            {
                tooltip->setCoord(0, 0, 450, 300); // this is the maximum width of the tooltip before it starts word-wrapping

                tooltipSize = MyGUI::IntSize(0, tooltip->getSize().height);
            }
            else
                tooltipSize = tooltip->getSize();

            std::map<std::string, std::string> userStrings = focus->getUserStrings();
            for (std::map<std::string, std::string>::iterator it = userStrings.begin();
                it != userStrings.end(); ++it)
            {
                if (it->first == "ToolTipType"
                    || it->first == "ToolTipLayout")
                    continue;


                size_t underscorePos = it->first.find("_");
                std::string propertyKey = it->first.substr(0, underscorePos);
                std::string widgetName = it->first.substr(underscorePos+1, it->first.size()-(underscorePos+1));

                MyGUI::Widget* w;
                getWidget(w, widgetName);
                w->setProperty(propertyKey, it->second);
            }

            for (unsigned int i=0; i<tooltip->getChildCount(); ++i)
            {
                MyGUI::Widget* w = tooltip->getChildAt(i);

                if (w->isUserString("AutoResizeHorizontal"))
                {
                    MyGUI::TextBox* text = w->castType<MyGUI::TextBox>();
                    tooltipSize.width = std::max(tooltipSize.width, w->getLeft() + text->getTextSize().width + 8);
                }
                else if (!tooltip->isUserString("DontResize"))
                    tooltipSize.width = std::max(tooltipSize.width, w->getLeft() + w->getWidth() + 8);

                if (w->isUserString("AutoResizeVertical"))
                {
                    MyGUI::TextBox* text = w->castType<MyGUI::TextBox>();
                    int height = text->getTextSize().height;
                    if (height > w->getHeight())
                    {
                        tooltipSize += MyGUI::IntSize(0, height - w->getHeight());
                    }
                    if (height < w->getHeight())
                    {
                        tooltipSize -= MyGUI::IntSize(0, w->getHeight() - height);
                    }
                }
            }
            tooltip->setCoord(0, 0, tooltipSize.width, tooltipSize.height);
        }
        else
            throw std::runtime_error ("unknown tooltip type");

        IntPoint tooltipPosition = InputManager::getInstance().getMousePosition() + IntPoint(0, 24);

        // make the tooltip stay completely in the viewport
        if ((tooltipPosition.left + tooltipSize.width) > viewSize.width)
        {
            tooltipPosition.left = viewSize.width - tooltipSize.width;
        }
        if ((tooltipPosition.top + tooltipSize.height) > viewSize.height)
        {
            tooltipPosition.top = viewSize.height - tooltipSize.height;
        }

        setCoord(tooltipPosition.left, tooltipPosition.top, tooltipSize.width, tooltipSize.height);
    }
    else
    {
        if (!mFocusObject.isEmpty())
        {
            IntSize tooltipSize = getToolTipViaPtr();

            setCoord(viewSize.width/2 - tooltipSize.width/2,
                    std::max(0, int(mFocusToolTipY*viewSize.height - tooltipSize.height)),
                    tooltipSize.width,
                    tooltipSize.height);

            mDynamicToolTipBox->setVisible(true);
        }
    }
}

void ToolTips::enterGameMode()
{
    mGameMode = true;
}

void ToolTips::enterGuiMode()
{
    mGameMode = false;
}

void ToolTips::setFocusObject(const MWWorld::Ptr& focus)
{
    mFocusObject = focus;
}

IntSize ToolTips::getToolTipViaPtr (bool image)
{
    // this the maximum width of the tooltip before it starts word-wrapping
    setCoord(0, 0, 300, 300);

    IntSize tooltipSize;

    const MWWorld::Class& object = MWWorld::Class::get (mFocusObject);
    if (!object.hasToolTip(mFocusObject))
    {
        mDynamicToolTipBox->setVisible(false);
    }
    else
    {
        mDynamicToolTipBox->setVisible(true);

        ToolTipInfo info = object.getToolTipInfo(mFocusObject);
        if (!image)
            info.icon = "";
        tooltipSize = createToolTip(info);
    }

    return tooltipSize;
}

void ToolTips::findImageExtension(std::string& image)
{
    int len = image.size();
    if (len < 4) return;

    if (!Ogre::ResourceGroupManager::getSingleton().resourceExists(Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME, image))
    {
        // Change texture extension to .dds
        image[len-3] = 'd';
        image[len-2] = 'd';
        image[len-1] = 's';
    }
}

IntSize ToolTips::createToolTip(const MWGui::ToolTipInfo& info)
{
    mDynamicToolTipBox->setVisible(true);

    std::string caption = info.caption;
    std::string image = info.icon;
    int imageSize = (image != "") ? 32 : 0;
    std::string text = info.text;

    // remove the first newline (easier this way)
    if (text.size() > 0 && text[0] == '\n')
        text.erase(0, 1);

    const ESM::Enchantment* enchant;
    const ESMS::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
    if (info.enchant != "")
    {
        enchant = store.enchants.search(info.enchant);
        if (enchant->data.type == ESM::Enchantment::CastOnce)
            text += "\n" + store.gameSettings.search("sItemCastOnce")->str;
        else if (enchant->data.type == ESM::Enchantment::WhenStrikes)
            text += "\n" + store.gameSettings.search("sItemCastWhenStrikes")->str;
        else if (enchant->data.type == ESM::Enchantment::WhenUsed)
            text += "\n" + store.gameSettings.search("sItemCastWhenUsed")->str;
        else if (enchant->data.type == ESM::Enchantment::ConstantEffect)
            text += "\n" + store.gameSettings.search("sItemCastConstant")->str;
    }

    // this the maximum width of the tooltip before it starts word-wrapping
    setCoord(0, 0, 300, 300);

    const IntPoint padding(8, 8);

    const int imageCaptionHPadding = (caption != "" ? 8 : 0);
    const int imageCaptionVPadding = (caption != "" ? 4 : 0);

    std::string realImage = "icons\\" + image;
    findImageExtension(realImage);

    EditBox* captionWidget = mDynamicToolTipBox->createWidget<EditBox>("NormalText", IntCoord(0, 0, 300, 300), Align::Left | Align::Top, "ToolTipCaption");
    captionWidget->setProperty("Static", "true");
    captionWidget->setCaption(caption);
    IntSize captionSize = captionWidget->getTextSize();

    int captionHeight = std::max(caption != "" ? captionSize.height : 0, imageSize);

    EditBox* textWidget = mDynamicToolTipBox->createWidget<EditBox>("SandText", IntCoord(0, captionHeight+imageCaptionVPadding, 300, 300-captionHeight-imageCaptionVPadding), Align::Stretch, "ToolTipText");
    textWidget->setProperty("Static", "true");
    textWidget->setProperty("MultiLine", "true");
    textWidget->setProperty("WordWrap", "true");
    textWidget->setCaption(text);
    textWidget->setTextAlign(Align::HCenter | Align::Top);
    IntSize textSize = textWidget->getTextSize();

    captionSize += IntSize(imageSize, 0); // adjust for image
    IntSize totalSize = IntSize( std::max(textSize.width, captionSize.width + ((image != "") ? imageCaptionHPadding : 0)),
        ((text != "") ? textSize.height + imageCaptionVPadding : 0) + captionHeight );

    if (info.effects != 0)
    {
        Widget* effectArea = mDynamicToolTipBox->createWidget<Widget>("",
            IntCoord(0, totalSize.height, 300, 300-totalSize.height),
            Align::Stretch, "ToolTipEffectArea");

        IntCoord coord(0, 6, totalSize.width, 24);

        /**
         * \todo
         * the various potion effects should appear in the tooltip depending if the player 
         * has enough skill in alchemy to know about the effects of this potion.
         */

        Widgets::MWEffectListPtr effectsWidget = effectArea->createWidget<Widgets::MWEffectList>
            ("MW_StatName", coord, Align::Default, "ToolTipEffectsWidget");
        effectsWidget->setWindowManager(mWindowManager);
        effectsWidget->setEffectList(info.effects);

        std::vector<MyGUI::WidgetPtr> effectItems;
        effectsWidget->createEffectWidgets(effectItems, effectArea, coord, true, Widgets::MWEffectList::EF_Potion);
        totalSize.height += coord.top-6;
        totalSize.width = std::max(totalSize.width, coord.width);
    }

    if (info.enchant != "")
    {
        Widget* enchantArea = mDynamicToolTipBox->createWidget<Widget>("",
            IntCoord(0, totalSize.height, 300, 300-totalSize.height),
            Align::Stretch, "ToolTipEnchantArea");

        IntCoord coord(0, 6, totalSize.width, 24);

        Widgets::MWEffectListPtr enchantWidget = enchantArea->createWidget<Widgets::MWEffectList>
            ("MW_StatName", coord, Align::Default, "ToolTipEnchantWidget");
        enchantWidget->setWindowManager(mWindowManager);
        enchantWidget->setEffectList(&enchant->effects);

        std::vector<MyGUI::WidgetPtr> enchantEffectItems;
        int flag = (enchant->data.type == ESM::Enchantment::ConstantEffect) ? Widgets::MWEffectList::EF_Constant : 0;
        enchantWidget->createEffectWidgets(enchantEffectItems, enchantArea, coord, true, flag);
        totalSize.height += coord.top-6;
        totalSize.width = std::max(totalSize.width, coord.width);

        if (enchant->data.type == ESM::Enchantment::WhenStrikes
            || enchant->data.type == ESM::Enchantment::WhenUsed)
        {
            /// \todo store the current enchantment charge somewhere
            int charge = enchant->data.charge;

            const int chargeWidth = 204;

            TextBox* chargeText = enchantArea->createWidget<TextBox>("SandText", IntCoord(0, 0, 10, 18), Align::Default, "ToolTipEnchantChargeText");
            chargeText->setCaption(store.gameSettings.search("sCharges")->str);
            chargeText->setProperty("Static", "true");
            const int chargeTextWidth = chargeText->getTextSize().width + 5;

            const int chargeAndTextWidth = chargeWidth + chargeTextWidth;

            totalSize.width = std::max(totalSize.width, chargeAndTextWidth);

            chargeText->setCoord((totalSize.width - chargeAndTextWidth)/2, coord.top+6, chargeTextWidth, 18);

            IntCoord chargeCoord;
            if (totalSize.width < chargeWidth)
            {
                totalSize.width = chargeWidth;
                chargeCoord = IntCoord(0, coord.top+6, chargeWidth, 18);
            }
            else
            {
                chargeCoord = IntCoord((totalSize.width - chargeAndTextWidth)/2 + chargeTextWidth, coord.top+6, chargeWidth, 18);
            }
            Widgets::MWDynamicStatPtr chargeWidget = enchantArea->createWidget<Widgets::MWDynamicStat>
                ("MW_ChargeBar", chargeCoord, Align::Default, "ToolTipEnchantCharge");
            chargeWidget->setValue(charge, charge);
            totalSize.height += 24;
        }
    }

    captionWidget->setCoord( (totalSize.width - captionSize.width)/2 + imageSize,
        (captionHeight-captionSize.height)/2,
        captionSize.width-imageSize,
        captionSize.height);

    captionWidget->setPosition (captionWidget->getPosition() + padding);
    textWidget->setPosition (textWidget->getPosition() + IntPoint(0, padding.top)); // only apply vertical padding, the horizontal works automatically due to Align::HCenter

    if (image != "")
    {
        ImageBox* imageWidget = mDynamicToolTipBox->createWidget<ImageBox>("ImageBox",
            IntCoord((totalSize.width - captionSize.width - imageCaptionHPadding)/2, 0, imageSize, imageSize),
            Align::Left | Align::Top, "ToolTipImage");
        imageWidget->setImageTexture(realImage);
        imageWidget->setPosition (imageWidget->getPosition() + padding);
    }

    totalSize += IntSize(padding.left*2, padding.top*2);

    return totalSize;
}

std::string ToolTips::toString(const float value)
{
    std::ostringstream stream;
    stream << std::setprecision(3) << value;
    return stream.str();
}

std::string ToolTips::toString(const int value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::string ToolTips::getValueString(const int value, const std::string& prefix)
{
    if (value == 0)
        return "";
    else
        return "\n" + prefix + ": " + toString(value);
}

std::string ToolTips::getMiscString(const std::string& text, const std::string& prefix)
{
    if (text == "")
        return "";
    else
        return "\n" + prefix + ": " + text;
}

std::string ToolTips::getCountString(const int value)
{
    if (value == 1)
        return "";
    else
        return " (" + boost::lexical_cast<std::string>(value) + ")";
}

void ToolTips::toggleFullHelp()
{
    mFullHelp = !mFullHelp;
}

bool ToolTips::getFullHelp() const
{
    return mFullHelp;
}

void ToolTips::setFocusObjectScreenCoords(float min_x, float min_y, float max_x, float max_y)
{
    mFocusToolTipX = (min_x + max_x) / 2;
    mFocusToolTipY = min_y;
}