import * as functions from "firebase-functions";

/**
 * Import function triggers from their respective submodules:
 *
 * import {onCall} from "firebase-functions/v2/https";
 * import {onDocumentWritten} from "firebase-functions/v2/firestore";
 *
 * See a full list of supported triggers at https://firebase.google.com/docs/functions
 */

import * as admin from "firebase-admin";

admin.initializeApp();

import * as account from "./modules/account";
import * as station from "./modules/station";

// Start writing functions
// https://firebase.google.com/docs/functions/typescript

export const helloWorld = functions.https.onRequest((request, response) => {
  response.send("Hello from Firebase!");
});

/* Account */
exports.accountOnCreate = account.accountOnCreate;
exports.accountCreateApiKey = account.accountCreateApiKey;
exports.accountDeleteApiKey = account.accountDeleteApiKey;
exports.accountGetApiKey = account.accountGetApiKey;

/* Station */
exports.stationCreate = station.stationCreate;
exports.stationUpdate = station.stationUpdate;
exports.stationDelete = station.stationDelete;
exports.stationGet = station.stationGet;
exports.stationList = station.stationList;
exports.stationApiCreate = station.stationApiCreate;
exports.stationApiUpdate = station.stationApiUpdate;
exports.stationApiDelete = station.stationApiDelete;
exports.stationApiGet = station.stationApiGet;
exports.stationApiList = station.stationApiList;
