import * as functions from "firebase-functions";
import * as admin from "firebase-admin";
import {UserRecord} from "firebase-admin/auth";
import {CallableContext} from "firebase-functions/v1/https";
import {AuthData} from "firebase-functions/lib/common/providers/https";

const db = admin.database();

export const accountOnCreate = functions.auth
  .user()
  .onCreate(async (user: UserRecord) => {
    const {uid, email, displayName, photoURL} = user;

    const privateUserRef = db.ref(`privateUsers/${uid}`);
    const publicUserRef = db.ref(`publicUsers/${uid}`);

    await privateUserRef.set({
      email,
      displayName,
      photoURL,
      lastLogin: new Date().toISOString(),
    });

    await publicUserRef.set({
      email,
      displayName,
      photoURL,
    });

    return;
  });

export const accountCreateApiKey = functions.https.onCall(
  async (data, context: CallableContext) => {
    const {auth} = context;
    const {uid} = auth as AuthData;

    const apiKeyRef = db.ref(`privateUsers/${uid}/apiKey`);
    const apiKey = apiKeyRef.push().key;

    await apiKeyRef.set(apiKey);

    return apiKey;
  }
);

export const accountDeleteApiKey = functions.https.onCall(
  async (data, context: CallableContext) => {
    const {auth} = context;
    const {uid} = auth as AuthData;

    const apiKeyRef = db.ref(`privateUsers/${uid}/apiKey`);

    await apiKeyRef.remove();

    return;
  }
);

export const accountGetApiKey = functions.https.onCall(
  async (data, context: CallableContext) => {
    const {auth} = context;
    const {uid} = auth as AuthData;

    const apiKeyRef = db.ref(`privateUsers/${uid}/apiKey`);

    const snapshot = await apiKeyRef.once("value");

    return snapshot.val();
  }
);
