#include "WindowGrouper.h"
#include "ActivationHistory.h"
#include "utils/Util.h"
#include "lifecycle/IconUtil.h"
#include "core/ConfigManager.h"
#include <algorithm>

namespace WindowGrouper {

    namespace {

        void populateFileDescriptionTokens(WindowGroup& group) {
            QString name = Util::getFileDescription(group.exePath);
            if (name.isEmpty()) return;
            QChar fl = Util::getDisplayFirstLetter(name);
            if (!fl.isNull())
                group.fileDescriptionTokens.insert(fl);
        }

        void populatePwaTokens(WindowGroup& group) {
            for (const auto& win : group.windows) {
                if (win.pwaDisplayName.isEmpty()) continue;
                QChar fl = Util::getDisplayFirstLetter(win.pwaDisplayName);
                if (!fl.isNull())
                    group.pwaNameTokens.insert(fl);
            }
        }

        void populateTitleTokens(WindowGroup& group) {
            for (const auto& win : group.windows) {
                if (win.title.isEmpty()) continue;
                QChar fl = Util::getDisplayFirstLetter(win.title);
                if (!fl.isNull())
                    group.titleTokens.insert(fl);
            }
        }

        void populateDisplayNameTokens(WindowGroup& group) {
            if (group.displayName.isEmpty()) return;
            QChar fl = Util::getDisplayFirstLetter(group.displayName);
            if (!fl.isNull())
                group.displayNameTokens.insert(fl);
        }

        void populateProcessNameTokens(WindowGroup&) {
            // Reserved for future use
        }

        void populateJumpTokens(WindowGroup& group) {
            populateDisplayNameTokens(group);

            group.allJumpTokens = group.displayNameTokens;
        }

    } // anonymous namespace

    QList<WindowGroup> groupWindows(const QList<WindowDescriptor>& windows,
                                    const ActivationHistory* history,
                                    HWND selfHwnd) {
        QList<WindowGroup> result;
        QMap<QString, int> indexMap;

        bool pwaEnabled = cfg().getPwaEnabled();
        bool separateGroups = pwaEnabled && cfg().getPwaMode() == PwaMode::SeparateGroups;
        bool ungroup = cfg().get("UngroupWindows", false).toBool();

        for (const auto& desc : windows) {
            if (desc.hwnd == selfHwnd) continue;
            if (desc.processPath.isEmpty()) continue;

            QString groupKey = desc.identity.groupKey();
            if (separateGroups && desc.windowKind == WindowKind::Pwa)
                groupKey = desc.appUserModelId;
            QString baseKey = groupKey;
            if (ungroup)
                groupKey += QStringLiteral("#") + QString::number(reinterpret_cast<quintptr>(desc.hwnd));

            WindowInfo winInfo{desc.title, desc.className, desc.appUserModelId,
                               desc.hwnd, desc.windowKind, desc.pwaDisplayName,
                               desc.identity};

            if (auto it = indexMap.find(groupKey); it != indexMap.end()) {
                result[it.value()].addWindow(winInfo);
            } else {
                WindowGroup group;
                group.exePath = desc.processPath;
                group.icon = Util::resolveWindowIcon(desc).icon;
                group.displayName = Util::resolveDisplayName(desc);
                group.baseKey = baseKey;

                group.addWindow(winInfo);
                populateJumpTokens(group);
                indexMap[groupKey] = result.size();
                result.append(group);
            }
        }

        if (history) {
            std::sort(result.begin(), result.end(),
                      [history](const WindowGroup& a, const WindowGroup& b) {
                qint64 latestA = 0, latestB = 0;
                for (const auto& win : a.windows)
                    latestA = qMax(latestA, history->lastActivationTime(win.identity));
                for (const auto& win : b.windows)
                    latestB = qMax(latestB, history->lastActivationTime(win.identity));
                return latestA > latestB;
            });
        }

        // UngroupWindows: number split windows of the same app 1, 2, 3... by creation order.
        // AltTaber remembers the order it first saw each window (kept until the window closes),
        // so a window keeps its number no matter which one is selected or used last.
        if (ungroup) {
            static QHash<quintptr, quint64> firstSeen;
            static quint64 nextSeq = 0;

            // Forget windows that no longer exist
            for (auto it = firstSeen.begin(); it != firstSeen.end();) {
                if (IsWindow(reinterpret_cast<HWND>(it.key()))) ++it;
                else it = firstSeen.erase(it);
            }

            // New windows get the next number; when several appear at once,
            // the least recently used one is treated as the oldest
            for (auto it = result.crbegin(); it != result.crend(); ++it) {
                const auto key = reinterpret_cast<quintptr>(it->windows.first().hwnd);
                if (!firstSeen.contains(key))
                    firstSeen.insert(key, nextSeq++);
            }

            QMap<QString, QList<quint64>> seqsByApp;
            for (const auto& g : result)
                seqsByApp[g.baseKey].append(firstSeen.value(reinterpret_cast<quintptr>(g.windows.first().hwnd)));
            for (auto& seqs : seqsByApp)
                std::sort(seqs.begin(), seqs.end());

            for (auto& g : result) {
                const auto& seqs = seqsByApp[g.baseKey];
                if (seqs.size() > 1)
                    g.splitIndex = static_cast<int>(seqs.indexOf(firstSeen.value(reinterpret_cast<quintptr>(g.windows.first().hwnd)))) + 1;
            }
        }

        return result;
    }

} // namespace WindowGrouper
