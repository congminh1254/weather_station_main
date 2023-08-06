import * as functions from "firebase-functions";
import * as admin from "firebase-admin";
import { CallableContext, HttpsError } from "firebase-functions/v1/https";

const db = admin.database();
const auth = admin.auth();

interface ApiEndpointConfig {
    method: "GET" | "POST" | "PUT" | "PATCH" | "DELETE";
    authRequired: boolean;
}

export class ApiManager {
  static getRequestContext = async (
    request: functions.https.Request,
    authRequired = true
  ): Promise<CallableContext> => {
    const authHeader = request.headers.authorization;
    let user = undefined;
    if (authHeader) {
      const token = authHeader.split("Bearer ")[1];
      const userData = (
        await db
          .ref("privateUsers")
          .orderByChild("apiKey")
          .equalTo(token)
          .once("value")
      ).val();
      if (userData) {
        const userData2 = await auth.getUser(Object.keys(userData)[0]);
        user = {
          uid: Object.keys(userData)[0],
          token: Object.assign(userData2, {
            iss: "https://identitytoolkit.googleapis.com/google.identity.identitytoolkit.v1.IdentityToolkit",
            aud: "https://identitytoolkit.googleapis.com/google.identity.identitytoolkit.v1.IdentityToolkit",
            auth_time: new Date().getTime() / 1000,
            user_id: Object.keys(userData)[0],
            sub: Object.keys(userData)[0],
            iat: new Date().getTime() / 1000,
            exp: (new Date().getTime() + 3600) / 1000,
            firebase: {
              identities: {},
              sign_in_provider: "custom",
            },
          }),
        };
      }
    }

    if (authRequired && !user) {
      throw new functions.https.HttpsError(
        "unauthenticated",
        "Authentication required"
      );
    }

    return {
      rawRequest: request,
      app: undefined,
      auth: user,
      instanceIdToken: undefined,
    };
  };

  static getRequestData = async (
    request: functions.https.Request
  ): Promise<any> => {
    let data = request.body;
    if (!data) {
      data = request.query;
    }
    return data;
  };

  static serializeError = (error: HttpsError): any => {
    return {
      success: false,
      error: {
        code: error.code,
        message: error.message,
        details: error.details,
      },
    };
  };

  static serializeResult = (result: any): any => {
    return {
      success: true,
      data: result,
    };
  };

  static handleResponse = async (
    request: functions.https.Request,
    response: functions.Response,
    caller: any,
    { method, authRequired }: ApiEndpointConfig = {
      method: "POST",
      authRequired: true,
    }
  ): Promise<any> => {
    try {
      const data = await ApiManager.getRequestData(request);
      const context = await ApiManager.getRequestContext(
        request,
        authRequired
      );
      if (method !== request.method) {
        throw new functions.https.HttpsError(
          "invalid-argument",
          "Invalid method"
        );
      }

      const result = await caller(data, context);
      const serializedResult = ApiManager.serializeResult(result);

      return response.send(serializedResult);
    } catch (error) {
      const httpsError = error as HttpsError;
      const errorDetails = this.serializeError(httpsError);
      switch (httpsError.code) {
      case "unauthenticated":
        return response.status(401).send(errorDetails);
      case "permission-denied":
        return response.status(403).send(errorDetails);
      case "not-found":
        return response.status(404).send(errorDetails);
      case "already-exists":
        return response.status(409).send(errorDetails);
      case "invalid-argument":
        return response.status(400).send(errorDetails);
      default:
        return response.status(500).send(errorDetails);
      }
    }
    //   .then((result: any) => {
    //     response.send(result);
    //   })
    //   .catch((error: any) => {
    //     response.send(error);
    //   });
  };
}
