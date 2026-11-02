#include <QDebug>
#include <QStyle>
#include "lifecycle/IconOnlyDelegate.h"
#include "WindowTypes.h"
#include "core/ThemeManager.h"

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto& colors = ThemeManager::current();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(colors.delegateSelected);
        painter->drawRoundedRect(option.rect, m_cornerRadius, m_cornerRadius);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(colors.delegateHover);
        painter->drawRoundedRect(option.rect, m_cornerRadius, m_cornerRadius);
    }

    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        QRect iconRect{{}, option.decorationSize};
        iconRect.moveCenter(option.rect.center());
        icon.paint(painter, iconRect);
    }

    const auto groupData = qvariant_cast<WindowGroup>(index.data(Qt::UserRole));
    // Split windows (UngroupWindows) show their number; grouped apps show their window count
    const int badgeNumber = groupData.splitIndex > 0
        ? groupData.splitIndex
        : (groupData.windows.size() > 1 ? static_cast<int>(groupData.windows.size()) : 0);
    if (badgeNumber > 0) {
        auto text = QString::number(badgeNumber);
        const auto badgeTextPadding = 16 * (text.size() - 1);
        constexpr auto badgeRadius = 24;
        auto badgeCenter = option.rect.topRight() + QPoint(-(badgeRadius + 6), badgeRadius + 6);
        auto badgeRect = QRect(badgeCenter + QPoint(-badgeRadius - badgeTextPadding, -badgeRadius), QSize(2 * badgeRadius + badgeTextPadding, 2 * badgeRadius));
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.badgeBg);
        painter->drawRoundedRect(badgeRect, badgeRadius, badgeRadius);

        painter->setFont(m_badgeFont);
        painter->setPen(colors.badgeText);
        painter->drawText(badgeRect, Qt::AlignCenter, text);
    }
}

QSize IconOnlyDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(160, 160);
}
