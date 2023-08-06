import * as functions from "firebase-functions";
import * as admin from "firebase-admin";
import { AuthData } from "firebase-functions/lib/common/providers/https";
import { ApiManager } from "../utils";

const db = admin.database();

interface Station {
	id?: string;
	name?: string;
	description?: string;
	owner?: string;
	lastOnline?: number;
	online?: boolean;
	isPublic?: boolean;
}

export class StationManager {
  static async create(
    data: Station,
    context: functions.https.CallableContext
  ): Promise<Station> {
    const { auth } = context;
    const { uid } = auth as AuthData;

    const { name, description } = data;

    const stationRef = db.ref("stations").push();

    await stationRef.set({
      name,
      description,
      owner: uid,
      isPublic: false,
    });

    const station = await stationRef.once("value");
    return station.val();
  }

  static async update(
    data: Station,
    context: functions.https.CallableContext
  ): Promise<Station> {
    const { auth } = context;
    const { uid } = auth as AuthData;

    const { id, name, description, isPublic } = data;

    const stationRef = db.ref(`stations/${id}`);

    const station = await stationRef.once("value");

    if (station.val().owner !== uid) {
      throw new functions.https.HttpsError(
        "permission-denied",
        "You are not the owner of this station"
      );
    }

    await stationRef.update({
      name,
      description,
      isPublic,
    });

    const updatedStation = await stationRef.once("value");
    return updatedStation.val();
  }

  static async delete(
    data: Station,
    context: functions.https.CallableContext
  ): Promise<void> {
    const { auth } = context;
    const { uid } = auth as AuthData;

    const { id } = data;

    const stationRef = db.ref(`stations/${id}`);

    const station = await stationRef.once("value");

    if (station.val().owner !== uid) {
      throw new functions.https.HttpsError(
        "permission-denied",
        "You are not the owner of this station"
      );
    }

    await stationRef.remove();

    return;
  }

  static async get(
    data: Station,
    context: functions.https.CallableContext
  ): Promise<Station> {
    const { auth } = context;
    const { uid } = auth as AuthData;

    const { id } = data;

    const stationRef = db.ref(`stations/${id}`);

    const station = await stationRef.once("value");

    if (station.val().owner !== uid && !station.val().isPublic) {
      throw new functions.https.HttpsError(
        "permission-denied",
        "You are not the owner of this station"
      );
    }

    return station.val();
  }

  static async list(
    data: Station,
    context: functions.https.CallableContext
  ): Promise<Station[]> {
    const { auth } = context;
    const { uid } = auth as AuthData;

    const stationsRef = db
      .ref("stations")
      .orderByChild("owner")
      .equalTo(uid);

    const stations = await stationsRef.once("value");

    return stations.val();
  }
}

export const stationCreate = functions.https.onCall(
  async (data: Station, context) => {
    return StationManager.create(data, context);
  }
);

export const stationUpdate = functions.https.onCall(
  async (data: Station, context) => {
    return StationManager.update(data, context);
  }
);

export const stationDelete = functions.https.onCall(
  async (data: Station, context) => {
    return StationManager.delete(data, context);
  }
);

export const stationGet = functions.https.onCall(
  async (data: Station, context) => {
    return StationManager.get(data, context);
  }
);

export const stationList = functions.https.onCall(async (data, context) => {
  return StationManager.list(data, context);
});

export const stationApiCreate = functions.https.onRequest(
  async (request, response) => {
    return ApiManager.handleResponse(
      request,
      response,
      StationManager.create,
    );
  }
);

export const stationApiUpdate = functions.https.onRequest(
  async (request, response) => {
    return ApiManager.handleResponse(
      request,
      response,
      StationManager.update,
      {
        method: "PATCH",
        authRequired: true,
      }
    );
  }
);

export const stationApiDelete = functions.https.onRequest(
  async (request, response) => {
    return ApiManager.handleResponse(
      request,
      response,
      StationManager.delete,
      {
        method: "DELETE",
        authRequired: true,
      }
    );
  }
);

export const stationApiGet = functions.https.onRequest(
  async (request, response) => {
    return ApiManager.handleResponse(
      request,
      response,
      StationManager.get,
      {
        method: "GET",
        authRequired: true,
      }
    );
  }
);

export const stationApiList = functions.https.onRequest(
  async (request, response) => {
    return ApiManager.handleResponse(
      request,
      response,
      StationManager.list,
      {
        method: "GET",
        authRequired: true,
      }
    );
  }
);
