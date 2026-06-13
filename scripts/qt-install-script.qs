// Qt unattended installation script for IFW v4.x
// Automatically installs: Qt 6.11.0 MinGW 64-bit + MinGW 13.1.0 + Qt SerialPort

function Controller() {
    // IFW v4: autoAcceptMessage/autoRejectMessage removed — not needed
}

Controller.prototype.WelcomePageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.CredentialsPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.IntroductionPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    gui.currentPageWidget().TargetDirectoryLineEdit.setText("D:/down/QT");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();
    widget.deselectAll();
    widget.selectComponent("qt.qt6.6110.win64_mingw");
    widget.selectComponent("qt.qt6.6110.addons.qtserialport.win64_mingw");
    widget.selectComponent("qt.tools.mingw1310");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ObligationsPageCallback = function() {
    var page = gui.currentPageWidget();
    page.AcceptCheckBox.checked = true;
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.LicenseAgreementPageCallback = function() {
    gui.currentPageWidget().AcceptLicenseRadioButton.setChecked(true);
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.StartMenuDirectoryPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ReadyForInstallationPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function() {
    var page = gui.currentPageWidget();
    try {
        if (page.LaunchQtCreatorCheckBoxForm) {
            page.LaunchQtCreatorCheckBoxForm.launchQtCreatorCheckBox.checked = false;
        }
    } catch (e) {}
    gui.clickButton(buttons.FinishButton);
}
