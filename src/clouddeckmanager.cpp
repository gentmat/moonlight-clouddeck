#include "clouddeckmanager.h"
#include <QWebEngineSettings>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>

CloudDeckManager::CloudDeckManager(QObject *parent)
    : QObject(parent)
    , m_webProfile(nullptr)
    , m_webPage(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_pollTimer(new QTimer(this))
    , m_formSubmitted(false)
    , m_loginInProgress(false)
    , m_webEngineInitialized(false)
    , m_parseStep(0)
{
    // Set up timeout timer
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000); // 30 second timeout
    
    // Set up poll timer for SPA detection
    m_pollTimer->setInterval(2000); // Check every 2 seconds
    
    // Connect timeout signal
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        m_pollTimer->stop();
        emit loginCompleted(false, "Login timeout");
        m_loginInProgress = false;
        m_formSubmitted = false;
    });
    
    // Connect poll timer to check page after form submission
    connect(m_pollTimer, &QTimer::timeout, this, &CloudDeckManager::checkPageAfterSubmission);
}

CloudDeckManager::~CloudDeckManager()
{
    if (m_webPage) {
        delete m_webPage;
    }
    if (m_webProfile) {
        delete m_webProfile;
    }
}

void CloudDeckManager::loginWithCredentials(const QString &email, const QString &password)
{
    qInfo() << "CloudDeck: loginWithCredentials called with email:" << email;
    
    if (m_loginInProgress) {
        qInfo() << "CloudDeck: Login already in progress, aborting";
        emit loginCompleted(false, "Login already in progress");
        return;
    }
    
    // Initialize web engine if not already done
    if (!m_webEngineInitialized) {
        qInfo() << "CloudDeck: Initializing web engine...";
        initializeWebEngine();
    }
    
    m_email = email;
    m_password = password;
    m_loginInProgress = true;
    
    qInfo() << "CloudDeck: Starting login process, navigating to CloudDeck portal...";
    
    // Start timeout timer
    m_timeoutTimer->start();
    
    // Navigate to CloudDeck login page
    m_webPage->load(QUrl("https://portal.clouddeck.app/login"));
}

void CloudDeckManager::onPageLoadFinished(bool ok)
{
    qInfo() << "CloudDeck: Page load finished, ok=" << ok << ", URL:" << m_webPage->url().toString();
    
    if (!ok) {
        qInfo() << "CloudDeck: Page load FAILED";
        emit loginCompleted(false, "Failed to load CloudDeck login page");
        m_loginInProgress = false;
        m_timeoutTimer->stop();
        return;
    }
    
    // Check if we're on the login page
    m_webPage->toHtml([this](const QString &html) {
        qInfo() << "CloudDeck: Analyzing page content, length:" << html.length();
        qInfo() << "CloudDeck: Page title:" << m_webPage->title();
        
        // Check for login page indicators
        bool hasEmailInput = html.contains("type=\"email\"");
        bool hasPasswordInput = html.contains("type=\"password\"");
        bool hasLoginForm = html.contains("input-dark") || html.contains("login");
        
        qInfo() << "CloudDeck: Page analysis - hasEmail:" << hasEmailInput << "hasPassword:" << hasPasswordInput << "hasLoginForm:" << hasLoginForm;
        
        if (hasEmailInput && hasPasswordInput && hasLoginForm) {
            // We're on the login page, fill the form
            qInfo() << "CloudDeck: Detected login page, filling form";
            fillLoginForm(m_email, m_password);
        } else {
            // We might be on the next page after login
            qInfo() << "CloudDeck: Not a login page - assuming post-login page";
            m_loginInProgress = false;
            m_timeoutTimer->stop();
            qInfo() << "CloudDeck: Login successful! Next page HTML content:";
            qInfo() << "=== NEXT PAGE HTML START ===";
            qInfo().noquote() << html;
            qInfo() << "=== NEXT PAGE HTML END ===";
            emit loginCompleted(true, "");
        }
    });
}

void CloudDeckManager::fillLoginForm(const QString &email, const QString &password)
{
    qInfo() << "CloudDeck: Filling login form with email:" << email;
    
    // JavaScript to fill and submit the login form (React/Vue compatible)
    QString script = QString(R"(
        (function() {
            console.log('=== CloudDeck Form Fill Starting ===');
            
            // Helper function to set input value in React/Vue compatible way
            function setNativeValue(element, value) {
                try {
                    const descriptor = Object.getOwnPropertyDescriptor(element, 'value');
                    const prototype = Object.getPrototypeOf(element);
                    const prototypeDescriptor = Object.getOwnPropertyDescriptor(prototype, 'value');
                    
                    if (descriptor && descriptor.set) {
                        if (prototypeDescriptor && prototypeDescriptor.set && descriptor.set !== prototypeDescriptor.set) {
                            prototypeDescriptor.set.call(element, value);
                        } else {
                            descriptor.set.call(element, value);
                        }
                    } else if (prototypeDescriptor && prototypeDescriptor.set) {
                        prototypeDescriptor.set.call(element, value);
                    } else {
                        element.value = value;
                    }
                } catch (e) {
                    console.log('setNativeValue fallback due to: ' + e.message);
                    element.value = value;
                }
            }
            
            // Helper to trigger all necessary events for React/Vue
            function triggerInputEvents(element) {
                element.dispatchEvent(new Event('input', { bubbles: true, cancelable: true }));
                element.dispatchEvent(new Event('change', { bubbles: true, cancelable: true }));
                element.dispatchEvent(new KeyboardEvent('keydown', { bubbles: true }));
                element.dispatchEvent(new KeyboardEvent('keyup', { bubbles: true }));
                element.dispatchEvent(new Event('blur', { bubbles: true }));
            }
            
            // Find and fill email input
            var emailInput = document.querySelector('input[type="email"]');
            if (emailInput) {
                console.log('Found email input: ' + emailInput.className);
                emailInput.focus();
                setNativeValue(emailInput, '%1');
                triggerInputEvents(emailInput);
                console.log('Email filled, value now: ' + emailInput.value);
            } else {
                console.log('ERROR: Email input not found!');
            }
            
            // Find and fill password input
            var passwordInput = document.querySelector('input[type="password"]');
            if (passwordInput) {
                console.log('Found password input: ' + passwordInput.className);
                passwordInput.focus();
                setNativeValue(passwordInput, '%2');
                triggerInputEvents(passwordInput);
                console.log('Password filled, value length: ' + passwordInput.value.length);
            } else {
                console.log('ERROR: Password input not found!');
            }
            
            // List all buttons for debugging
            var allButtons = document.querySelectorAll('button');
            console.log('Found ' + allButtons.length + ' buttons:');
            for (var b = 0; b < allButtons.length; b++) {
                console.log('  Button ' + b + ': type=' + allButtons[b].type + ', text="' + allButtons[b].textContent.trim().substring(0,30) + '"');
            }
            
            // Wait for React/Vue to process state changes, then submit
            setTimeout(function() {
                console.log('=== Attempting form submission ===');
                
                // Try multiple submit strategies
                var submitButton = document.querySelector('button[type="submit"]');
                console.log('button[type=submit] found: ' + !!submitButton);
                
                if (!submitButton) {
                    // Look for any button with login-related text
                    var buttons = document.querySelectorAll('button');
                    for (var i = 0; i < buttons.length; i++) {
                        var text = buttons[i].textContent.toLowerCase();
                        if (text.includes('login') || text.includes('sign in') || text.includes('log in')) {
                            submitButton = buttons[i];
                            console.log('Found button by text: "' + text.trim() + '"');
                            break;
                        }
                    }
                }
                
                if (submitButton) {
                    console.log('Clicking submit button...');
                    submitButton.focus();
                    submitButton.click();
                    console.log('Button clicked!');
                } else {
                    console.log('No submit button found, trying form.submit()');
                    var form = document.querySelector('form');
                    if (form) {
                        console.log('Found form, submitting...');
                        form.submit();
                    } else {
                        console.log('ERROR: No form found either!');
                    }
                }
            }, 500);
            
            return 'Form fill initiated';
        })();
    )").arg(email).arg(password);
    
    m_webPage->runJavaScript(script, [this](const QVariant &result) {
        qInfo() << "CloudDeck: Form fill result:" << result.toString();
        // Start polling for page changes (SPA doesn't trigger loadFinished)
        m_formSubmitted = true;
        m_pollTimer->start();
        qInfo() << "CloudDeck: Started polling for page changes after form submission";
    });
}

bool CloudDeckManager::hasStoredCredentials()
{
    QSettings settings;
    return settings.contains("clouddeck/email") && settings.contains("clouddeck/password");
}

void CloudDeckManager::clearStoredCredentials()
{
    QSettings settings;
    settings.remove("clouddeck/email");
    settings.remove("clouddeck/password");
}

void CloudDeckManager::initializeWebEngine()
{
    if (m_webEngineInitialized) {
        return;
    }
    
    qInfo() << "CloudDeck: Creating QWebEnginePage (headless, no widget)...";
    
    // Initialize web engine components without QWidget
    m_webProfile = new QWebEngineProfile(this);
    m_webPage = new QWebEnginePage(m_webProfile, this);
    
    qInfo() << "CloudDeck: Configuring web engine settings...";
    
    // Configure web engine settings
    m_webPage->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    m_webPage->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    
    // Connect signals
    connect(m_webPage, &QWebEnginePage::loadFinished, this, &CloudDeckManager::onPageLoadFinished);

    m_webEngineInitialized = true;
    qInfo() << "CloudDeck: Web engine initialized successfully";
}

void CloudDeckManager::checkPageAfterSubmission()
{
    qInfo() << "CloudDeck: Polling - checking page state after form submission...";
    qInfo() << "CloudDeck: Current URL:" << m_webPage->url().toString();
    
    m_webPage->toHtml([this](const QString &html) {
        qInfo() << "CloudDeck: Poll - Page content length:" << html.length();
        
        // Check if we're still on the login page
        bool hasEmailInput = html.contains("type=\"email\"");
        bool hasPasswordInput = html.contains("type=\"password\"");
        
        qInfo() << "CloudDeck: Poll - hasEmail:" << hasEmailInput << "hasPassword:" << hasPasswordInput;
        
        if (!hasEmailInput || !hasPasswordInput) {
            // We've navigated away from login page - success!
            m_pollTimer->stop();
            m_timeoutTimer->stop();
            m_loginInProgress = false;
            m_formSubmitted = false;
            
            qInfo() << "CloudDeck: Login successful! Now parsing dashboard...";
            emit loginCompleted(true, "");
            
            // Start parsing the dashboard
            m_parseStep = 0;
            parseDashboard();
        } else {
            // Still on login page - check for error messages
            if (html.contains("error") || html.contains("invalid") || html.contains("incorrect")) {
                qInfo() << "CloudDeck: Possible login error detected on page";
            } else {
                qInfo() << "CloudDeck: Still on login page, waiting...";
            }
        }
    });
}

void CloudDeckManager::onLoginFormFilled()
{
    // Slot called when login form is filled
    // Currently handled by fillLoginForm() directly
}

void CloudDeckManager::parseDashboard()
{
    qInfo() << "CloudDeck: Parsing dashboard (step" << m_parseStep << ")...";
    
    // Wait for SPA content to fully load
    QTimer::singleShot(2000, this, [this]() {
        qInfo() << "CloudDeck: Running dashboard parse after delay...";
        
        // JavaScript to extract machine info and click Show password
        QString script = R"(
            (function() {
                var result = {};
                result.debug = [];
                
                // Debug: Check what elements exist
                result.debug.push('app-machine-status: ' + !!document.querySelector('app-machine-status'));
                result.debug.push('app-machine-info: ' + !!document.querySelector('app-machine-info'));
                result.debug.push('app-dashboard: ' + !!document.querySelector('app-dashboard'));
                
                // Get full body text for extraction
                var bodyText = document.body.innerText;
                result.debug.push('Body text length: ' + bodyText.length);
                
                // Extract machine status from app-machine-status or body text
                var statusElement = document.querySelector('app-machine-status');
                if (statusElement) {
                    result.status = statusElement.textContent.trim();
                } else if (bodyText.includes('Running')) {
                    result.status = 'Running';
                } else if (bodyText.includes('Stopped')) {
                    result.status = 'Stopped';
                } else {
                    result.status = 'Unknown';
                }
                
                // Extract session duration from body text
                var durationMatch = bodyText.match(/Session Duration[:\s]*(\d+h)/i);
                if (durationMatch) {
                    result.sessionDuration = durationMatch[1];
                } else {
                    // Try simpler pattern - just look for "Xh" pattern
                    var hourMatch = bodyText.match(/(\d+h)/);
                    if (hourMatch) {
                        result.sessionDuration = hourMatch[1];
                    } else {
                        result.sessionDuration = 'Unknown';
                    }
                }
                
                // Click Show button to reveal password
                var spans = document.querySelectorAll('span');
                for (var j = 0; j < spans.length; j++) {
                    if (spans[j].textContent.trim() === 'Show') {
                        spans[j].click();
                        result.clickedShow = true;
                        break;
                    }
                }
                
                return JSON.stringify(result);
            })();
        )";
        
        m_webPage->runJavaScript(script, [this](const QVariant &result) {
            QString jsonStr = result.toString();
            qInfo() << "CloudDeck: Dashboard parse result:" << jsonStr;
            
            // Parse the JSON result
            if (jsonStr.contains("status")) {
                // Extract status
                int statusStart = jsonStr.indexOf("\"status\":\"") + 10;
                int statusEnd = jsonStr.indexOf("\"", statusStart);
                if (statusStart > 9 && statusEnd > statusStart) {
                    m_machineStatus = jsonStr.mid(statusStart, statusEnd - statusStart);
                }
                
                // Extract session duration
                int durationStart = jsonStr.indexOf("\"sessionDuration\":\"") + 19;
                int durationEnd = jsonStr.indexOf("\"", durationStart);
                if (durationStart > 18 && durationEnd > durationStart) {
                    m_sessionDuration = jsonStr.mid(durationStart, durationEnd - durationStart);
                }
            }
            
            qInfo() << "CloudDeck: Status:" << m_machineStatus;
            qInfo() << "CloudDeck: Session Duration:" << m_sessionDuration;
            
            // Wait for SPA to update after clicking Show, then get password
            QTimer::singleShot(2500, this, &CloudDeckManager::clickShowPassword);
        });
    });
}

void CloudDeckManager::clickShowPassword()
{
    qInfo() << "CloudDeck: Getting password after Show click...";
    
    QString script = R"(
        (function() {
            var result = {};
            
            // Check if pre.inline element exists (password visible)
            var preElement = document.querySelector('pre.inline');
            result.preFound = !!preElement;
            
            if (preElement) {
                result.password = preElement.textContent.trim();
                return JSON.stringify(result);
            }
            
            // Check if Show button still exists (password not yet visible)
            var showButton = null;
            var spans = document.querySelectorAll('span');
            for (var j = 0; j < spans.length; j++) {
                if (spans[j].textContent.trim() === 'Show') {
                    showButton = spans[j];
                    break;
                }
            }
            result.showButtonExists = !!showButton;
            
            // If Show button exists, click it
            if (showButton) {
                showButton.click();
                result.clickedShow = true;
            }
            
            result.password = '';
            return JSON.stringify(result);
        })();
    )";
    
    m_webPage->runJavaScript(script, [this](const QVariant &result) {
        QString jsonStr = result.toString();
        qInfo() << "CloudDeck: Password check result:" << jsonStr;
        
        // Extract password from JSON
        if (jsonStr.contains("\"password\":\"") && !jsonStr.contains("\"password\":\"\"")) {
            int pwStart = jsonStr.indexOf("\"password\":\"") + 12;
            int pwEnd = jsonStr.indexOf("\"", pwStart);
            if (pwStart > 11 && pwEnd > pwStart) {
                m_userPassword = jsonStr.mid(pwStart, pwEnd - pwStart);
            }
        }
        
        if (m_userPassword.isEmpty()) {
            qInfo() << "CloudDeck: Password not visible yet, trying again...";
            // Try clicking Show again
            QString clickScript = R"(
                (function() {
                    var spans = document.querySelectorAll('span');
                    for (var j = 0; j < spans.length; j++) {
                        if (spans[j].textContent.trim() === 'Show') {
                            spans[j].click();
                            return 'clicked';
                        }
                    }
                    return 'not found';
                })();
            )";
            m_webPage->runJavaScript(clickScript, [this](const QVariant &clickResult) {
                qInfo() << "CloudDeck: Click Show result:" << clickResult.toString();
                QTimer::singleShot(2000, this, [this]() {
                    // Try one more time to get the password from <pre> element
                    QString getPasswordScript = R"(
                        (function() {
                            var preElement = document.querySelector('pre.inline');
                            if (preElement) {
                                return preElement.textContent.trim();
                            }
                            return 'password_not_found';
                        })();
                    )";
                    m_webPage->runJavaScript(getPasswordScript, [this](const QVariant &pwResult) {
                        m_userPassword = pwResult.toString();
                        qInfo() << "CloudDeck: Final password extraction:" << m_userPassword;
                        printMachineInfo();
                    });
                });
            });
        } else {
            printMachineInfo();
        }
    });
}

void CloudDeckManager::clickConnectButton()
{
    qInfo() << "CloudDeck: Clicking Connect button...";
    
    QString script = R"(
        (function() {
            // Find and click the Connect button
            var buttons = document.querySelectorAll('button');
            for (var i = 0; i < buttons.length; i++) {
                if (buttons[i].textContent.includes('Connect')) {
                    buttons[i].click();
                    return 'clicked';
                }
            }
            return 'not_found';
        })();
    )";
    
    m_webPage->runJavaScript(script, [this](const QVariant &result) {
        qInfo() << "CloudDeck: Connect button click result:" << result.toString();
        
        if (result.toString() == "clicked") {
            // Wait for connection dialog to appear, then extract server address
            QTimer::singleShot(2000, this, &CloudDeckManager::extractServerAddress);
        } else {
            qInfo() << "CloudDeck: Connect button not found";
        }
    });
}

void CloudDeckManager::extractServerAddress()
{
    qInfo() << "CloudDeck: Extracting server address from connection dialog...";
    
    QString script = R"(
        (function() {
            var result = {};
            
            // Look for the server address in div.input-dark > span.text-white
            var inputDark = document.querySelector('div.input-dark');
            if (inputDark) {
                var span = inputDark.querySelector('span.text-white');
                if (span) {
                    result.serverAddress = span.textContent.trim();
                }
            }
            
            // Check if connection dialog is visible
            var connectionInfo = document.querySelector('app-connection-info');
            result.dialogVisible = !!connectionInfo;
            
            return JSON.stringify(result);
        })();
    )";
    
    m_webPage->runJavaScript(script, [this](const QVariant &result) {
        QString jsonStr = result.toString();
        qInfo() << "CloudDeck: Server address extraction result:" << jsonStr;
        
        // Extract server address from JSON
        if (jsonStr.contains("\"serverAddress\":\"")) {
            int start = jsonStr.indexOf("\"serverAddress\":\"") + 17;
            int end = jsonStr.indexOf("\"", start);
            if (start > 16 && end > start) {
                m_serverAddress = jsonStr.mid(start, end - start);
                qInfo() << "CloudDeck: Server address:" << m_serverAddress;
                
                // Emit signal with server address - Moonlight will add host and generate PIN
                emit serverAddressReady(m_serverAddress);
            }
        } else {
            qInfo() << "CloudDeck: Server address not found, dialog may not be open yet";
            // Retry after a short delay
            QTimer::singleShot(1000, this, &CloudDeckManager::extractServerAddress);
        }
    });
}

void CloudDeckManager::enterPinAndPair(const QString &pin)
{
    qInfo() << "CloudDeck: Entering PIN" << pin << "and clicking Pair...";
    
    QString script = QString(R"(
        (function() {
            var result = {};
            
            // Find the PIN input field
            var pinInput = document.querySelector('input.input-dark[maxlength="4"]');
            if (pinInput) {
                // Set value using native setter for Angular compatibility
                var nativeInputValueSetter = Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype, 'value').set;
                nativeInputValueSetter.call(pinInput, '%1');
                
                // Dispatch input event
                pinInput.dispatchEvent(new Event('input', { bubbles: true }));
                pinInput.dispatchEvent(new Event('change', { bubbles: true }));
                
                result.pinEntered = true;
            } else {
                result.pinEntered = false;
                result.error = 'PIN input not found';
            }
            
            return JSON.stringify(result);
        })();
    )").arg(pin);
    
    m_webPage->runJavaScript(script, [this, pin](const QVariant &result) {
        qInfo() << "CloudDeck: PIN entry result:" << result.toString();
        
        // Wait a moment for Angular to enable the Pair button, then click it
        QTimer::singleShot(500, this, [this]() {
            QString clickPairScript = R"(
                (function() {
                    var buttons = document.querySelectorAll('button');
                    for (var i = 0; i < buttons.length; i++) {
                        if (buttons[i].textContent.includes('Pair')) {
                            // Remove disabled attribute if present
                            buttons[i].removeAttribute('disabled');
                            buttons[i].click();
                            return 'clicked';
                        }
                    }
                    return 'not_found';
                })();
            )";
            
            m_webPage->runJavaScript(clickPairScript, [this](const QVariant &clickResult) {
                qInfo() << "CloudDeck: Pair button click result:" << clickResult.toString();
                
                if (clickResult.toString() == "clicked") {
                    qInfo() << "CloudDeck: Pairing initiated on CloudDeck side";
                    // The pairing completion will be handled by Moonlight's pairingCompleted signal
                }
            });
        });
    });
}

void CloudDeckManager::printMachineInfo()
{
    qInfo() << "";
    qInfo() << "╔══════════════════════════════════════════════════════════════╗";
    qInfo() << "║                    CLOUDDECK MACHINE INFO                    ║";
    qInfo() << "╠══════════════════════════════════════════════════════════════╣";
    qInfo() << "║  Status:           " << m_machineStatus.leftJustified(42) << "║";
    qInfo() << "║  Session Duration: " << m_sessionDuration.leftJustified(42) << "║";
    qInfo() << "║  User Password:    " << m_userPassword.leftJustified(42) << "║";
    qInfo() << "╚══════════════════════════════════════════════════════════════╝";
    qInfo() << "";
    
    emit machineInfoReady(m_machineStatus, m_userPassword, m_sessionDuration);
    
    // Auto-connect: Click Connect button to get server address
    QTimer::singleShot(1000, this, &CloudDeckManager::clickConnectButton);
}
