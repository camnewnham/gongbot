# gongbot
A robot that rings a gong triggered by remote events (slack responses or slash commands, stripe sales, etc.)

Runs on a Lolin D1 Mini with a mini-servo. Long polls a server and rings the gong when the server responds.

![image](https://user-images.githubusercontent.com/19278856/210203932-382ac68d-0739-48b2-a180-2e348bcedfef.png)
![image](https://user-images.githubusercontent.com/19278856/210203969-d3051c0a-2879-4a6b-b1d8-28093932cf6e.png)

An excerpt of the server side featuring:
1. Endpoint for webhooks that emits an event
2. Slack reaction handler for slack apps
3. Listener endpoint for devices waiting for events

```ts
import { EventEmitter } from "events";
import { Request, Response } from "express";

const emitter = new EventEmitter();

export const emitEvent = async (req: Request, res: Response) => {
  const count = emitEventName(req.params["event"]);

  let responseText =
    count === 0
      ? `Nothing was listening for the ${req.params["event"]}`
      : count === 1
      ? `Released the ${req.params["event"]}`
      : `Released ${count} ${req.params["event"]}s`;

  res.status(200).send(responseText);
};

export const emitEventName = (name: string) => {
  const count = emitter.listenerCount(name);
  console.info(`Release ${count} listeners on ${name}.`);
  emitter.emit(name);
  emitter.removeAllListeners();
  return count;
};

/**
 * Long polling endpoint.
 * This function will return when the matching item is fired.
 */
export const listenForEvent = (req: Request, res: Response) => {
  res.chunkedEncoding = true;
  console.info(
    `Waiting for ${req.params["event"]}... ${
      emitter.listenerCount(req.params["event"]) + 1
    } total listeners.`
  );

  const trigger = () => {
    res.write("OK");
    res.end();
  };

  emitter.on(req.params["event"], trigger);

  // Heroku kills connections after 30s for the first connection
  // Or 55s for ongoing connections that do not transfer any data.
  const keepAlive = setInterval(() => {
    res.write(" ", (err) => {
      if (err) {
        console.info(
          `Unable to keep listener alive on ${req.params.event}: ${err?.message}`
        );
        close();
      }
    });
  }, 50000);

  const close = () => {
    emitter.removeListener(req.params["event"], trigger);
    clearInterval(keepAlive);
  };
  res.on("close", close);

  res.write(" ");
};

export const onSlackReaction = (req: Request, res: Response) => {
  if (req.body.type === "url_verification") {
    return res.status(200).send(req.body.challenge);
  }

  // Release any listeners that have the emoji name
  if (req.body.event?.type === "reaction_added") {
    console.info(`Slack reaction ${req.body.event?.reaction}`);
    emitEventName(req.body.event.reaction);
  }

  return res.status(200).send();
};
```
