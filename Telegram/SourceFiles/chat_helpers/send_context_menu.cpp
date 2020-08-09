/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "chat_helpers/send_context_menu.h"

#include "api/api_common.h"
#include "base/event_filter.h"
#include "core/shortcuts.h"
#include "lang/lang_keys.h"
#include "ui/widgets/popup_menu.h"

#include <QtWidgets/QApplication>

FillMenuResult FillSendMenu(
		not_null<Ui::PopupMenu*> menu,
		Fn<SendMenuType()> type,
		Fn<void()> silent,
		Fn<void()> schedule) {
	if (!silent && !schedule) {
		return FillMenuResult::None;
	}
	const auto now = type();
	if (now == SendMenuType::Disabled
		|| (!silent && now == SendMenuType::SilentOnly)) {
		return FillMenuResult::None;
	}

	if (silent && now != SendMenuType::Reminder) {
		menu->addAction(tr::lng_send_silent_message(tr::now), silent);
	}
	if (schedule && now != SendMenuType::SilentOnly) {
		menu->addAction(
			(now == SendMenuType::Reminder
				? tr::lng_reminder_message(tr::now)
				: tr::lng_schedule_message(tr::now)),
			schedule);
	}
	return FillMenuResult::Success;
}

void SetupSendMenuAndShortcuts(
		not_null<Ui::RpWidget*> button,
		Fn<SendMenuType()> type,
		Fn<void()> silent,
		Fn<void()> schedule) {
	if (!silent && !schedule) {
		return;
	}
	const auto menu = std::make_shared<base::unique_qptr<Ui::PopupMenu>>();
	const auto showMenu = [=] {
		*menu = base::make_unique_q<Ui::PopupMenu>(button);
		const auto result = FillSendMenu(*menu, type, silent, schedule);
		const auto success = (result == FillMenuResult::Success);
		if (success) {
			(*menu)->popup(QCursor::pos());
		}
		return success;
	};
	base::install_event_filter(button, [=](not_null<QEvent*> e) {
		if (e->type() == QEvent::ContextMenu && showMenu()) {
			return base::EventFilterResult::Cancel;
		}
		return base::EventFilterResult::Continue;
	});

	Shortcuts::Requests(
	) | rpl::start_with_next([=](not_null<Shortcuts::Request*> request) {
		using Command = Shortcuts::Command;

		const auto now = type();
		if (now == SendMenuType::Disabled
			|| (!silent && now == SendMenuType::SilentOnly)) {
			return;
		}
		(silent
			&& (now != SendMenuType::Reminder)
			&& request->check(Command::SendSilentMessage)
			&& request->handle([=] {
				silent();
				return true;
			}))
		||
		(schedule
			&& (now != SendMenuType::SilentOnly)
			&& request->check(Command::ScheduleMessage)
			&& request->handle([=] {
				schedule();
				return true;
			}))
		||
		(request->check(Command::JustSendMessage) && request->handle([=] {
			const auto post = [&](QEvent::Type type) {
				QApplication::postEvent(
					button,
					new QMouseEvent(
						type,
						QPointF(0, 0),
						Qt::LeftButton,
						Qt::LeftButton,
						Qt::NoModifier));
			};
			post(QEvent::MouseButtonPress);
			post(QEvent::MouseButtonRelease);
			return true;
		}));
	}, button->lifetime());
}
