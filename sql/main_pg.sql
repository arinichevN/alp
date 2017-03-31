
DROP SCHEMA if exists alp CASCADE;
CREATE SCHEMA alp;


CREATE TABLE alp.config
(
  app_class character varying(32) NOT NULL,
  db_public character varying(256) NOT NULL,
  udp_port character varying(32) NOT NULL,
  pid_path character varying(32) NOT NULL,
  udp_buf_size character varying(32) NOT NULL,
  db_data character varying(32) NOT NULL,
  db_log character varying(32) NOT NULL,
  cycle_duration_us character varying(32) NOT NULL,
  sms_peer_id character varying(32) NOT NULL,
  CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE alp.prog
(
  id integer NOT NULL,
  param_id integer NOT NULL,
  peer_id character varying(32) NOT NULL,
  message character varying(256) NOT NULL,
  active integer NOT NULL,
  CONSTRAINT prog_pkey PRIMARY KEY (id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE alp.param
(
  id integer NOT NULL,
  check_interval interval NOT NULL default '600',
  cope_duration interval NOT NULL default '300',
  phone_number_group_id integer NOT NULL,
  CONSTRAINT param_pkey PRIMARY KEY (id)
)
WITH (
  OIDS=FALSE
);



