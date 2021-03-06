#include "stdafx.h"


#include "../../controls/TextUnit.h"
#include "../../controls/CustomButton.h"
#include "../../controls/ContactAvatarWidget.h"
#include "../../fonts.h"
#include "../../app_config.h"

#include "../../utils/log/log.h"
#include "../../utils/utils.h"
#include "../../styles/ThemeParameters.h"
#include "../../main_window/contact_list/ContactListModel.h"
#include "../../main_window/sidebar/SidebarUtils.h"
#include "../../cache/avatars/AvatarStorage.h"
#include "../mediatype.h"

#include "ChatEventInfo.h"
#include "MessageStyle.h"
#include "ChatEventItem.h"

namespace Ui
{
    namespace
    {
        qint32 getBubbleHorMargin(int _width)
        {
            if (_width >= Ui::MessageStyle::fiveHeadsWidth())
                return Utils::scale_value(136);

            if (_width >= Ui::MessageStyle::fourHeadsWidth())
                return Utils::scale_value(116);

            if (_width >= Ui::MessageStyle::threeHeadsWidth())
                return Utils::scale_value(96);

            return Utils::scale_value(32);
        }

        qint32 getTextHorPadding()
        {
            return Utils::scale_value(8);
        }

        qint32 getTextTopPadding()
        {
            return Utils::scale_value(4);
        }

        qint32 getTextBottomPadding()
        {
            return Utils::scale_value(5);//ask Andrew to explain
        }

        qint32 getButtonsHeight()
        {
            return Utils::scale_value(40);
        }

        qint32 getButtonsRadius()
        {
            return Utils::scale_value(12);
        }

        qint32 getButtonsSpacing()
        {
            return Utils::scale_value(8);
        }

        constexpr auto fontSize() noexcept { return 12; }
    }

    ChatEventItem::ChatEventItem(const HistoryControl::ChatEventInfoSptr& _eventInfo, const qint64 _id, const qint64 _prevId)
        : HistoryControlPageItem(nullptr)
        , EventInfo_(_eventInfo)
        , id_(_id)
        , prevId_(_prevId)
    {
        assert(EventInfo_);

        init();
    }

    ChatEventItem::ChatEventItem(QWidget* _parent, const HistoryControl::ChatEventInfoSptr& _eventInfo, const qint64 _id, const qint64 _prevId)
        : HistoryControlPageItem(_parent)
        , EventInfo_(_eventInfo)
        , id_(_id)
        , prevId_(_prevId)
    {
        assert(EventInfo_);

        const auto showLinks = EventInfo_->isCaptchaPresent() ? TextRendering::LinksVisible::SHOW_LINKS : TextRendering::LinksVisible::DONT_SHOW_LINKS;
        TextWidget_ = TextRendering::MakeTextUnit(EventInfo_->formatEventText(), {}, showLinks);
        TextWidget_->applyLinks(EventInfo_->getMembersLinks());
        initTextWidget();
        init();
    }

    ChatEventItem::ChatEventItem(QWidget* _parent, std::unique_ptr<TextRendering::TextUnit> _textUnit)
        : HistoryControlPageItem(_parent)
        , TextWidget_(std::move(_textUnit))
        , id_(-1)
        , prevId_(-1)
    {
        init();
    }

    ChatEventItem::~ChatEventItem() = default;

    void ChatEventItem::init()
    {
        setMouseTracking(true);
        setMultiselectEnabled(false);

        if (parentWidget())
        {
            auto layout = Utils::emptyVLayout(this);
            layout->setAlignment(Qt::AlignHCenter);
            textPlaceholder_ = new QWidget(this);
            layout->addWidget(textPlaceholder_);

            updateButtons();

            connect(Ui::GetDispatcher(), &Ui::core_dispatcher::chatInfo, this, &ChatEventItem::onChatInfo);
            connect(Ui::GetDispatcher(), &Ui::core_dispatcher::modChatAboutResult, this, &ChatEventItem::modChatAboutResult);
            connect(Logic::GetAvatarStorage(), &Logic::AvatarStorage::avatarChanged, this, &ChatEventItem::avatarChanged);
        }
    }

    void ChatEventItem::initTextWidget()
    {
        const auto color = getTextColor(getContact());
        const auto linkColor = getLinkColor(getContact());
        TextWidget_->init(getTextFont(), color, linkColor, QColor(), QColor(), TextRendering::HorAligment::CENTER);
        TextWidget_->applyFontToLinks(getTextFontBold());
    }

    QString ChatEventItem::formatRecentsText() const
    {
        if (EventInfo_)
            return EventInfo_->formatEventText();

        if (TextWidget_)
            return TextWidget_->getText();

        return QString();
    }

    MediaType ChatEventItem::getMediaType(MediaRequestMode) const
    {
        return MediaType::noMedia;
    }

    void ChatEventItem::setLastStatus(LastStatus _lastStatus)
    {
        if (getLastStatus() != _lastStatus)
        {
            assert(_lastStatus == LastStatus::None);
            HistoryControlPageItem::setLastStatus(_lastStatus);
            updateGeometry();
            update();
        }
    }

    int32_t ChatEventItem::evaluateTextWidth(const int32_t _widgetWidth)
    {
        assert(_widgetWidth > 0);

        const auto maxBubbleWidth = _widgetWidth - 2 * getBubbleHorMargin(_widgetWidth);
        const auto maxBubbleContentWidth = maxBubbleWidth - 2 * getTextHorPadding();

        return maxBubbleContentWidth;
    }

    void ChatEventItem::updateStyle()
    {
        TextWidget_->setColor(getTextColor(getContact()));
        update();
    }

    void ChatEventItem::updateFonts()
    {
        initTextWidget();
        updateSize();
        update();
    }

    void ChatEventItem::updateSize()
    {
        updateSize(size());
    }

    void ChatEventItem::updateSize(const QSize& _newSize)
    {
        updateButtons();

        // setup the text control and get it dimensions
        const auto maxTextWidth = evaluateTextWidth(_newSize.width());
        const auto textHeight = TextWidget_->getHeight(maxTextWidth);
        const auto textWidth = TextWidget_->getMaxLineWidth();

        // evaluate bubble width
        auto bubbleWidth = textWidth + 2 * getTextHorPadding();
        if (hasButtons())
        {
            auto visibleCount = 0;
            auto btnWidth = 0;

            const auto buttons = buttonsWidget_->findChildren<RoundButton*>();
            for (auto btn : buttons)
            {
                if (btn->isVisible())
                {
                    ++visibleCount;
                    btnWidth = std::max(btnWidth, btn->textDesiredWidth());
                }
            }
            const auto buttonsDesiredWidth = btnWidth * visibleCount + getButtonsSpacing() * (visibleCount + 1);
            bubbleWidth = std::max(bubbleWidth, buttonsDesiredWidth);
            buttonsWidget_->setFixedWidth(bubbleWidth);
        }

        // evaluate bubble height
        auto bubbleHeight = textHeight + getTextTopPadding() + getTextBottomPadding();
        const auto topPadding = Utils::scale_value(8);
        if (hasButtons())
        {
            textPlaceholder_->setFixedHeight(bubbleHeight);
            bubbleHeight += buttonsWidget_->height() - topPadding - bottomOffset();
        }

        BubbleRect_ = QRect(0, 0, bubbleWidth, bubbleHeight);
        BubbleRect_.moveCenter(QRect(0, 0, _newSize.width(), bubbleHeight).center());

        BubbleRect_.moveTop(topPadding);

        // setup geometry
        height_ = bubbleHeight + topPadding + bottomOffset();
        if (!hasButtons())
            textPlaceholder_->setFixedHeight(height_);

        TextWidget_->setOffsets((width() - TextWidget_->cachedSize().width()) / 2, BubbleRect_.top() + getTextTopPadding());
    }

    void ChatEventItem::initButtons(const QString& _leftCaption, const QString& _rightCaption)
    {
        assert(!buttonsWidget_);
        buttonsWidget_ = new QWidget(this);
        buttonsWidget_->setFixedHeight(getButtonsHeight());

        const auto bgNormal = Styling::getParameters().getColor(Styling::StyleVariable::CHAT_PRIMARY);
        const auto bgHover = Styling::getParameters().getColor(Styling::StyleVariable::CHAT_PRIMARY_HOVER);
        const auto bgActive = Styling::getParameters().getColor(Styling::StyleVariable::CHAT_PRIMARY_ACTIVE);
        const auto textColor = Styling::getParameters().getColor(Styling::StyleVariable::TEXT_PRIMARY);

        auto hLayout = Utils::emptyHLayout(buttonsWidget_);
        hLayout->setSpacing(getButtonsSpacing());
        hLayout->setContentsMargins(getButtonsSpacing(), 0, getButtonsSpacing(), 0);
        btnLeft_ = new RoundButton(buttonsWidget_, getButtonsRadius());
        btnLeft_->setText(_leftCaption);
        btnLeft_->setTextColor(textColor);
        btnLeft_->setColors(bgNormal, bgHover, bgActive);
        btnLeft_->setFixedHeight(getButtonsHeight());
        hLayout->addWidget(btnLeft_);

        btnRight_ = new RoundButton(buttonsWidget_, getButtonsRadius());
        btnRight_->setText(_rightCaption);
        btnRight_->setTextColor(textColor);
        btnRight_->setColors(bgNormal, bgHover, bgActive);
        btnRight_->setFixedHeight(getButtonsHeight());
        hLayout->addWidget(btnRight_);

        layout()->addWidget(buttonsWidget_);
    }

    bool ChatEventItem::hasButtons() const
    {
        return buttonsWidget_ && buttonsVisible_;
    }

    void ChatEventItem::updateButtons()
    {
        if (!EventInfo_ || EventInfo_->eventType() != core::chat_event_type::mchat_invite)
            return;

        buttonsVisible_ = false;
        if (const auto& aimId = getContact(); Logic::getContactListModel()->isYouAdmin(aimId))
        {
            const auto descriptionButtonVisible = Logic::getContactListModel()->getChatDescription(aimId).isEmpty();
            const auto avatarButtonVisible = Logic::GetAvatarStorage()->isDefaultAvatar(aimId);
            buttonsVisible_ = avatarButtonVisible || descriptionButtonVisible;

            if (buttonsVisible_ && !buttonsWidget_)
            {
                initButtons(QT_TRANSLATE_NOOP("chat_event", "Add avatar"), QT_TRANSLATE_NOOP("chat_event", "Add description"));
                connect(btnLeft_, &RoundButton::clicked, this, &ChatEventItem::addAvatarClicked);
                connect(btnRight_, &RoundButton::clicked, this, &ChatEventItem::addDescriptionClicked);
            }

            if (btnLeft_)
                btnLeft_->setVisible(avatarButtonVisible);
            if (btnRight_)
                btnRight_->setVisible(descriptionButtonVisible);
        }

        if (buttonsWidget_)
            buttonsWidget_->setVisible(buttonsVisible_);
    }

    bool ChatEventItem::isOutgoing() const
    {
        return false;
    }

    int32_t ChatEventItem::getTime() const
    {
        return 0;
    }

    int ChatEventItem::bottomOffset() const
    {
        auto margin = Utils::scale_value(2);

        if (isChat() && hasHeads() && headsAtBottom())
            margin += MessageStyle::getLastReadAvatarSize() + MessageStyle::getLastReadAvatarOffset();

        return margin;
    }

    QColor ChatEventItem::getTextColor(const QString& _contact)
    {
        return Styling::getParameters(_contact).getColor(Styling::StyleVariable::CHATEVENT_TEXT);
    }

    QColor ChatEventItem::getLinkColor(const QString& _contact)
    {
        return Styling::getParameters(_contact).getColor(Styling::StyleVariable::TEXT_PRIMARY);
    }

    QFont ChatEventItem::getTextFont()
    {
        return Fonts::adjustedAppFont(fontSize(), Fonts::defaultAppFontWeight(), Fonts::FontAdjust::NoAdjust);
    }

    QFont ChatEventItem::getTextFontBold()
    {
        return Fonts::adjustedAppFont(fontSize(), Fonts::FontWeight::SemiBold, Fonts::FontAdjust::NoAdjust);
    }

    void ChatEventItem::onChatInfo()
    {
        updateSize();
    }

    void ChatEventItem::modChatAboutResult(qint64 _seq, int)
    {
        if (_seq == modChatAboutSeq_)
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            const auto& aimid = getContact();
            collection.set_value_as_qstring("aimid", aimid);

            if (aimid.isEmpty())
            {
                const auto stamp = Logic::getContactListModel()->getChatStamp(aimid);
                if (stamp.isEmpty())
                    return;

                collection.set_value_as_qstring("stamp", stamp);
            }

            collection.set_value_as_int("limit", 0);
            Ui::GetDispatcher()->post_message_to_core("chats/info/get", collection.get());
        }
    }

    void ChatEventItem::avatarChanged(const QString&)
    {
        updateSize();
    }

    void ChatEventItem::addAvatarClicked()
    {
        auto avatar = std::make_unique<ContactAvatarWidget>(this, getContact(), QString(), 0, true);
        avatar->selectFileForAvatar();
        auto pixmap = avatar->croppedImage();
        if (!pixmap.isNull())
            avatar->applyAvatar(pixmap);
    }

    void ChatEventItem::addDescriptionClicked()
    {
        const auto model = Logic::getContactListModel();
        const auto& aimId = getContact();

        QString name = model->getChatName(aimId);
        QString description = model->getChatDescription(aimId);
        QString rules = model->getChatRules(aimId);

        auto avatar = editGroup(aimId, name, description, rules);
        if (name != model->getChatName(aimId))
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", aimId);
            collection.set_value_as_qstring("name", name);
            Ui::GetDispatcher()->post_message_to_core("chats/mod/name", collection.get());
        }

        if (description != model->getChatDescription(aimId))
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", aimId);
            collection.set_value_as_qstring("about", description);
            modChatAboutSeq_ = Ui::GetDispatcher()->post_message_to_core("chats/mod/about", collection.get());
        }

        if (rules != model->getChatRules(aimId))
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("aimid", aimId);
            collection.set_value_as_qstring("rules", rules);
            Ui::GetDispatcher()->post_message_to_core("chats/mod/rules", collection.get());
        }

        if (!avatar.isNull())
        {
            const auto byteArray = avatarToByteArray(avatar);

            Ui::gui_coll_helper helper(GetDispatcher()->create_collection(), true);

            core::ifptr<core::istream> data_stream(helper->create_stream());
            if (!byteArray.isEmpty())
                data_stream->write((const uint8_t*)byteArray.data(), (uint32_t)byteArray.size());

            helper.set_value_as_stream("avatar", data_stream.get());
            helper.set_value_as_qstring("aimid", aimId);

            GetDispatcher()->post_message_to_core("set_avatar", helper.get());
        }
    }

    void ChatEventItem::paintEvent(QPaintEvent*)
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
        p.setPen(Qt::NoPen);

        p.setBrush(Styling::getParameters(getContact()).getColor(Styling::StyleVariable::CHATEVENT_BACKGROUND));
        p.drawRoundedRect(BubbleRect_, MessageStyle::getBorderRadius(), MessageStyle::getBorderRadius());

        if (hasButtons())
            TextWidget_->draw(p);
        else
            TextWidget_->drawSmart(p, BubbleRect_.center().y());

        drawHeads(p);

        if (Q_UNLIKELY(Ui::GetAppConfig().IsShowMsgIdsEnabled()))
        {
            p.setPen(TextWidget_->getColor());
            p.setFont(Fonts::appFontScaled(10, Fonts::FontWeight::SemiBold));

            const auto x = BubbleRect_.right() + 1 - MessageStyle::getBorderRadius();
            const auto y = BubbleRect_.bottom() + 1;
            Utils::drawText(p, QPointF(x, y), Qt::AlignRight | Qt::AlignBottom, QString::number(getId()));
        }
    }

    QSize ChatEventItem::sizeHint() const
    {
        return QSize(0, height_);
    }

    void ChatEventItem::resizeEvent(QResizeEvent* _event)
    {
        HistoryControlPageItem::resizeEvent(_event);
        updateSize(_event->size());
    }

    void ChatEventItem::mousePressEvent(QMouseEvent* _event)
    {
        pressPos_ = _event->pos();
        HistoryControlPageItem::mousePressEvent(_event);
    }

    void ChatEventItem::mouseMoveEvent(QMouseEvent* _event)
    {
        if (TextWidget_->isOverLink(_event->pos()))
            setCursor(Qt::PointingHandCursor);
        else
            setCursor(Qt::ArrowCursor);

        HistoryControlPageItem::mouseMoveEvent(_event);
    }

    void ChatEventItem::mouseReleaseEvent(QMouseEvent* _event)
    {
        if (Utils::clicked(pressPos_, _event->pos()))
            TextWidget_->clicked(_event->pos());

        HistoryControlPageItem::mouseReleaseEvent(_event);
    }

    void ChatEventItem::clearSelection(bool)
    {
        TextWidget_->clearSelection();
    }
};
