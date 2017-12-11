CREATE TABLE "prog"
(
    "id" INTEGER PRIMARY KEY NOT NULL,
    "description" TEXT NOT NULL,
    "peer_id" TEXT NOT NULL,
    "check_interval" INTEGER NOT NULL,
    "cope_duration" INTEGER NOT NULL,
    "phone_number_group_id" INTEGER NOT NULL,
    "sms" INTEGER NOT NULL,
    "ring" INTEGER NOT NULL,
    "enable" INTEGER NOT NULL,
    "load" INTEGER NOT NULL
);
