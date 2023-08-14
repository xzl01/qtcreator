// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "authwidget.h"

#include "copilotclient.h"
#include "copilottr.h"

#include <utils/layoutbuilder.h>
#include <utils/stringutils.h>

#include <languageclient/languageclientmanager.h>

#include <QDesktopServices>
#include <QMessageBox>

using namespace LanguageClient;
using namespace Copilot::Internal;

namespace Copilot {

AuthWidget::AuthWidget(QWidget *parent)
    : QWidget(parent)
{
    using namespace Layouting;

    m_button = new QPushButton(Tr::tr("Sign In"));
    m_button->setEnabled(false);
    m_progressIndicator = new Utils::ProgressIndicator(Utils::ProgressIndicatorSize::Small);
    m_progressIndicator->setVisible(false);
    m_statusLabel = new QLabel();
    m_statusLabel->setVisible(false);
    m_statusLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse
                                           | Qt::TextInteractionFlag::TextSelectableByKeyboard);

    // clang-format off
    Column {
        Row {
            m_button, m_progressIndicator, st
        },
        m_statusLabel
    }.attachTo(this);
    // clang-format on

    connect(m_button, &QPushButton::clicked, this, [this]() {
        if (m_status == Status::SignedIn)
            signOut();
        else if (m_status == Status::SignedOut)
            signIn();
    });
}

AuthWidget::~AuthWidget()
{
    if (m_client)
        LanguageClientManager::shutdownClient(m_client);
}

void AuthWidget::setState(const QString &buttonText, bool working)
{
    m_button->setText(buttonText);
    m_button->setVisible(true);
    m_progressIndicator->setVisible(working);
    m_statusLabel->setVisible(!m_statusLabel->text().isEmpty());
    m_button->setEnabled(!working);
}

void AuthWidget::checkStatus()
{
    QTC_ASSERT(m_client && m_client->reachable(), return);

    setState("Checking status ...", true);

    m_client->requestCheckStatus(false, [this](const CheckStatusRequest::Response &response) {
        if (response.error()) {
            setState("failed: " + response.error()->message(), false);
            return;
        }
        const CheckStatusResponse result = *response.result();

        if (result.user().isEmpty()) {
            setState("Sign in", false);
            m_status = Status::SignedOut;
            return;
        }

        setState("Sign out " + result.user(), false);
        m_status = Status::SignedIn;
    });
}

void AuthWidget::updateClient(const Utils::FilePath &nodeJs, const Utils::FilePath &agent)
{
    LanguageClientManager::shutdownClient(m_client);
    m_client = nullptr;
    setState(Tr::tr("Sign In"), false);
    m_button->setEnabled(false);
    if (!nodeJs.isExecutableFile() || !agent.exists()) {
        return;
    }

    setState(Tr::tr("Sign In"), true);

    m_client = new CopilotClient(nodeJs, agent);
    connect(m_client, &Client::initialized, this, &AuthWidget::checkStatus);
}

void AuthWidget::signIn()
{
    qCritical() << "Not implemented";
    QTC_ASSERT(m_client && m_client->reachable(), return);

    setState("Signing in ...", true);

    m_client->requestSignInInitiate([this](const SignInInitiateRequest::Response &response) {
        QTC_ASSERT(!response.error(), return);

        Utils::setClipboardAndSelection(response.result()->userCode());

        QDesktopServices::openUrl(QUrl(response.result()->verificationUri()));

        m_statusLabel->setText(Tr::tr("A browser window will open. Enter the code %1 when "
                                      "asked.\nThe code has been copied to your clipboard.")
                                   .arg(response.result()->userCode()));
        m_statusLabel->setVisible(true);

        m_client
            ->requestSignInConfirm(response.result()->userCode(),
                                   [this](const SignInConfirmRequest::Response &response) {
                                       m_statusLabel->setText("");

                                       if (response.error()) {
                                           QMessageBox::critical(this,
                                                                 Tr::tr("Login Failed"),
                                                                 Tr::tr(
                                                                     "The login request failed: ")
                                                                     + response.error()->message());
                                           setState("Sign in", false);
                                           return;
                                       }

                                       setState("Sign Out " + response.result()->user(), false);
                                   });
    });
}

void AuthWidget::signOut()
{
    QTC_ASSERT(m_client && m_client->reachable(), return);

    setState("Signing out ...", true);

    m_client->requestSignOut([this](const SignOutRequest::Response &response) {
        QTC_ASSERT(!response.error(), return);
        QTC_ASSERT(response.result()->status() == "NotSignedIn", return);

        checkStatus();
    });
}

} // namespace Copilot
