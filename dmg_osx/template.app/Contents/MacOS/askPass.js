#!/usr/bin/env osascript -l JavaScript

// Setup application for scripting
const app = Application.currentApplication();
app.includeStandardAdditions = true;

const dialogOptions = {
  buttons: ['Cancel', 'Ok'],
  defaultAnswer: '',
  defaultButton: 'Ok',
  hiddenAnswer: true,
  withIcon: 'caution',
};

try {
  const result = app.displayDialog('Unraid USB creator wants to make changes. Type your password to allow this.', dialogOptions);

  if (result.buttonReturned === 'Ok') {
    result.textReturned
  }
}
catch (e) {
  // Canceled
}
