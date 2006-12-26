/*
 * Copyright (C) 2004-2006 Nikolay Pultsin <geometer@mawhrin.net>
 * Copyright (C) 2005 Mikhail Sobolev <mss@mawhrin.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <qapplication.h>
#include <qmessagebox.h>

#include "ZLQtDialogManager.h"
#include "ZLQtCommonDialog.h"
#include "ZLQtOptionsDialog.h"
#include "ZLQtSelectionDialog.h"
#include "ZLQtWaitMessage.h"

#include "../application/ZLQtApplicationWindow.h"

void ZLQtDialogManager::createApplicationWindow(ZLApplication *application) const {
	myApplicationWindow = new ZLQtApplicationWindow(application);
}

void ZLQtDialogManager::fullScreenWorkaround() const {
	if (myApplicationWindow != 0) {
		myApplicationWindow->fullScreenWorkaround();
	}
}

ZLOptionsDialog *ZLQtDialogManager::createOptionsDialog(const std::string &id, const std::string &title) const {
	return new ZLQtOptionsDialog(id, title);
}

ZLDialog *ZLQtDialogManager::createDialog(const std::string &title) const {
	return new ZLQtCommonDialog(title);
}

int ZLQtDialogManager::infoBox(const InfoBoxType type, const std::string &title, const std::string &message, const std::string &button0, const std::string &button1, const std::string &button2) const {
	int code = 0;
	switch (type) {
		default:
		case INFORMATION_TYPE:
		case QUESTION_TYPE:
			code = QMessageBox::information(qApp->mainWidget(), QString::fromUtf8(title.c_str()), QString::fromUtf8(message.c_str()), button0.c_str(), button1.c_str(), button2.c_str());
			break;
		case ERROR_TYPE:
			code = QMessageBox::critical(qApp->mainWidget(), QString::fromUtf8(title.c_str()), QString::fromUtf8(message.c_str()), button0.c_str(), button1.c_str(), button2.c_str());
			break;
	}
	fullScreenWorkaround();
	return code;
}

bool ZLQtDialogManager::selectionDialog(const std::string &title, ZLTreeHandler &handler) const {
	bool result = ZLQtSelectionDialog(title.c_str(), handler).run();
	fullScreenWorkaround();
	return result;
}

void ZLQtDialogManager::wait(ZLRunnable &runnable, const std::string &message) const {
	ZLQtWaitMessage waitMessage(message);
	runnable.run();
	fullScreenWorkaround();
}