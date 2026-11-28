// SPDX-License-Identifier: GPL-2.0
/*
 * Question and warning boxes of the OBS source (profiles): Qt message
 * boxes over the window in front (the source's properties), else OBS's
 * main window. Called from the user interface thread only (property
 * buttons).
 */

#include <obs-frontend-api.h>

#include <QApplication>
#include <QMessageBox>
#include <QWidget>

static QWidget *gchd_parent()
{
	QWidget *w = QApplication::activeWindow();

	return w ? w : static_cast<QWidget *>(obs_frontend_get_main_window());
}

/* TEXT as a question with Yes and No; true for Yes. */
extern "C" bool gchd_ask(const char *title, const char *text)
{
	return QMessageBox::question(gchd_parent(), QString::fromUtf8(title),
				     QString::fromUtf8(text),
				     QMessageBox::Yes | QMessageBox::No,
				     QMessageBox::No) == QMessageBox::Yes;
}

/* TEXT as a warning with OK. */
extern "C" void gchd_warn(const char *title, const char *text)
{
	QMessageBox::warning(gchd_parent(), QString::fromUtf8(title),
			     QString::fromUtf8(text));
}
