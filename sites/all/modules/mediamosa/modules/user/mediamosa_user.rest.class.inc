<?php
/**
 * @file
 * REST call user module.
 */

/**
 * URI: /user/$user_id
 *
 * Method: GET
 *
 * 1.x: user_management_get_user
 *
 */
class mediamosa_rest_call_user_get extends mediamosa_rest_call {
  // ------------------------------------------------------------------ Consts.
  // Rest vars;
  const USER_ID = 'user_id';

  // ------------------------------------------------------------------ Get Var Setup.
  public function get_var_setup() {
    $a_var_setup = array();

    $a_var_setup = array(
      self::VARS => array(
        self::USER_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_USER_ID,
          self::VAR_DESCRIPTION => 'The user to retrieve.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_YES,
          self::VAR_RANGE_END => mediamosa_user_db::NAME_LENGTH,
        ),
      )
    );

    // Enrich with required REST vars.
    return self::get_var_setup_default($a_var_setup);
  }

  // ------------------------------------------------------------------ Do Call.
  public function do_call() {
    $o_mediamosa = mediamosa::get();

    $a_app_ids = $this->get_param_value_app();
    $app_id = reset($a_app_ids);

    $user_id = $this->get_param_value(self::USER_ID);

    // Get the user quota.
    $result = mediamosa_user::get_user_quota($app_id, $user_id);

    // Add to response.
    $o_mediamosa->add_item($result);
  }
}

/**
 * URI: /user
 *
 * Method: GET
 *
 * 1.x: user_management_list_user
 *
 */
class mediamosa_rest_call_user_list extends mediamosa_rest_call {
  // ------------------------------------------------------------------ Consts.
  // Rest vars;
  const GROUP_ID = 'group_id';

  // ------------------------------------------------------------------ Get Var Setup.
  public function get_var_setup() {
    $a_var_setup = array();

    $a_var_setup = array(
      self::VARS => array(
        self::GROUP_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_GROUP_ID,
          self::VAR_DESCRIPTION => 'Name of the group where the user has to be a member.',
          self::VAR_RANGE_END => mediamosa_user_group_db::GROUP_ID_LENGTH,
        ),
      )
    );

    // Enrich with range.
    $a_var_setup = self::get_var_setup_range($a_var_setup);

    // Enrich with required REST vars.
    return self::get_var_setup_default($a_var_setup);
  }

  // ------------------------------------------------------------------ Do Call.
  public function do_call() {
    $o_mediamosa = mediamosa::get();

    $a_app_ids = $this->get_param_value_app();
    $app_id = reset($a_app_ids);

    $group_id = $this->get_param_value(self::GROUP_ID);
    $offset = $this->get_param_value_offset();
    $limit = $this->get_param_value_limit();

    // Fetch the items.
    $result = mediamosa_user::get_all(TRUE, $app_id, $offset, $limit);

    // Fill the response.
    foreach ($result as $row) {
      // TODO: maybe we shall optimize this by caching groups.
      $groups = mediamosa_user::get_groups($app_id, $row['user_id'], TRUE);
      if (!empty($group_id)) {
        // We need only menbers of certain group.
        if (!array_search($group_id, $groups)) {
          continue;
        }
      }
      if (!empty($groups)) {
        $row['group_id'] = $groups;
      }
      $o_mediamosa->add_item($row);
    }

    // Set total count.
    $o_mediamosa->item_count_total = $result->found_rows;
  }
}

/**
 * URI: /user/create
 *
 * Method: POST
 *
 * 1.x: user_management_create_user
 *
 */
class mediamosa_rest_call_user_create extends mediamosa_rest_call {
  // ------------------------------------------------------------------ Consts.
  // Rest vars;
  const USER_ID = 'user';
  const GROUP_ID = 'group_id';
  const QUOTUM = 'quotum';

  // ------------------------------------------------------------------ Get Var Setup.
  public function get_var_setup() {
    $a_var_setup = array();

    $a_var_setup = array(
      self::VARS => array(
        self::USER_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_USER_ID,
          self::VAR_DESCRIPTION => 'User name.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_YES,
          self::VAR_RANGE_END => mediamosa_user_db::NAME_LENGTH,
        ),
        self::GROUP_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_GROUP_ID,
          self::VAR_IS_ARRAY => self::VAR_IS_ARRAY_YES,
          self::VAR_DESCRIPTION => 'Name of the group the user belongs to. The group must exist (see /group/create [POST])',
          self::VAR_RANGE_END => mediamosa_user_group_db::GROUP_ID_LENGTH,
        ),
        self::QUOTUM => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_INT,
          self::VAR_DESCRIPTION => 'Maximum disk space of the user in mega bytes. A quotum of 0 means unlimited diskspace. Quota is is enforced with a soft-quota, a user is allowed to exceed the maximum, but the next action is rejected.',
          self::VAR_DEFAULT_VALUE => 0,
        ),
      )
    );

    // Enrich with required REST vars.
    return self::get_var_setup_default($a_var_setup);
  }

  // ------------------------------------------------------------------ Do Call.
  public function do_call() {
    $o_mediamosa = mediamosa::get();

    $a_app_ids = $this->get_param_value_app();
    $app_id = reset($a_app_ids);

    $user_id = $this->get_param_value(self::USER_ID);
    $group_id = $this->get_param_value(self::GROUP_ID);
    $quotum = $this->get_param_value(self::QUOTUM);

    if ($this->isset_given_param(self::GROUP_ID)) {
      mediamosa_db::db_must_exists(mediamosa_user_group_db::TABLE_NAME, array(mediamosa_user_group_db::ID => $group_id, mediamosa_user_group_db::APP_ID => $app_id));
    }

    // Make sure it does not exist.
    if (mediamosa_db::db_count_rows(mediamosa_user_db::TABLE_NAME, array(mediamosa_user_db::ID => $user_id, mediamosa_user_group_db::APP_ID => $app_id))) {
      throw new mediamosa_exception_error(mediamosa_error::ERRORCODE_USERMAN_USER_EXISTS);
    }

    // Create the user.
    mediamosa_user::create($app_id, $user_id, $quotum, $group_id);

    // Set Ok.
    $o_mediamosa->set_result_okay();
  }
}

/**
 * URI: /user/$user_id/delete
 *
 * Method: GET
 *
 * 1.x: user_management_delete_user
 *
 */
class mediamosa_rest_call_user_delete extends mediamosa_rest_call {
  // ------------------------------------------------------------------ Consts.
  // Rest vars;
  const USER_ID = 'user_id';

  // ------------------------------------------------------------------ Get Var Setup.
  public function get_var_setup() {
    $a_var_setup = array();

    $a_var_setup = array(
      self::VARS => array(
        self::USER_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_USER_ID,
          self::VAR_DESCRIPTION => 'The user to delete.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_YES,
          self::VAR_RANGE_END => mediamosa_user_db::NAME_LENGTH,
        ),
      )
    );

    // Enrich with required REST vars.
    return self::get_var_setup_default($a_var_setup);
  }

  // ------------------------------------------------------------------ Do Call.
  public function do_call() {
    $o_mediamosa = mediamosa::get();

    $a_app_ids = $this->get_param_value_app();
    $app_id = reset($a_app_ids);

    $user_id = $this->get_param_value(self::USER_ID);

    // Must exist.
    mediamosa_db::db_must_exists(mediamosa_user_db::TABLE_NAME, array(mediamosa_user_db::ID => $user_id, mediamosa_user_db::APP_ID => $app_id));

    // Delete the user.
    mediamosa_user::delete($app_id, $user_id);

    // Set Ok.
    $o_mediamosa->set_result_okay();
  }
}

/**
 * URI: /user/$user_id
 *
 * Method: POST
 *
 */
class mediamosa_rest_call_user_update extends mediamosa_rest_call {
  // ------------------------------------------------------------------ Consts.
  // Rest vars;
  const USER_ID = 'user_id';
  const GROUP_ID = 'group_id';
  const QUOTUM = 'quotum';
  const AUTO_CREATE = 'auto_create';

  // ------------------------------------------------------------------ Get Var Setup.
  public function get_var_setup() {
    $a_var_setup = array();

    $a_var_setup = array(
      self::VARS => array(
        self::USER_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_USER_ID,
          self::VAR_DESCRIPTION => 'User name or other indentifier.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_YES,
          self::VAR_RANGE_END => mediamosa_user_db::NAME_LENGTH,
        ),
        self::GROUP_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_GROUP_ID,
          self::VAR_IS_ARRAY => self::VAR_IS_ARRAY_YES,
          self::VAR_DESCRIPTION => 'A list of the groups in which to link the user to. Will replace all existing links when supplied. Supply empty array to remove from all groups.',
          self::VAR_RANGE_END => mediamosa_user_group_db::GROUP_ID_LENGTH,
        ),
        self::QUOTUM => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_INT,
          self::VAR_DESCRIPTION => 'The upload quotum of the user in megabytes.',
        ),
        self::AUTO_CREATE => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_BOOL,
          self::VAR_DESCRIPTION => 'Supply TRUE to create user when not found.',
          self::VAR_DEFAULT_VALUE => 'FALSE',
        ),
      )
    );

    // Enrich with required REST vars.
    return self::get_var_setup_default($a_var_setup);
  }

  // ------------------------------------------------------------------ Do Call.
  public function do_call() {
    $o_mediamosa = mediamosa::get();

    $app_ids = $this->get_param_value_app();
    $app_id = reset($app_ids);

    $user_id = $this->get_param_value(self::USER_ID);
    $group_ids = $this->get_param_value(self::GROUP_ID);
    $quotum = $this->get_param_value(self::QUOTUM);
    $auto_create = $this->get_param_value(self::AUTO_CREATE);

    // Either one must be given.
    if (!$this->isset_given_param(self::GROUP_ID) && !$this->isset_given_param(self::QUOTUM)) {
      throw new mediamosa_exception_error_empty_result();
    }

    if ($auto_create && !mediamosa_db::db_exists(mediamosa_user_db::TABLE_NAME, array(mediamosa_user_db::ID => $user_id, mediamosa_user_db::APP_ID => $app_id))) {
      mediamosa_user::create($app_id, $user_id, (empty($quotum) ? 0 : $quotum), $group_ids);
    }

    mediamosa_db::db_must_exists(mediamosa_user_db::TABLE_NAME, array(mediamosa_user_db::ID => $user_id, mediamosa_user_db::APP_ID => $app_id));

    if ($this->isset_given_param(self::GROUP_ID)) {
      foreach ($group_ids as $group_id) {
        mediamosa_db::db_must_exists(mediamosa_user_group_db::TABLE_NAME, array(mediamosa_user_group_db::ID => $group_id, mediamosa_user_group_db::APP_ID => $app_id));
      }
    }

    // Update the user.
    mediamosa_user::update($app_id, $user_id, $quotum, $group_ids);

    // Set Ok.
    $o_mediamosa->set_result_okay();
  }
}

/**
 * URI: /user/$user_id/set_group
 * (deprecated)
 *
 * Method: POST
 *
 * 1.x: user_management_set_group
 *
 */
class mediamosa_rest_call_user_set_group extends mediamosa_rest_call_user_update {
}

/**
 * URI: /user/$user_id/set_quotum
 * (deprecated)
 *
 * Method: POST
 *
 * 1.x: user_management_set_user_quotum
 *
 */
class mediamosa_rest_call_user_set_quotum extends mediamosa_rest_call_user_update {
}
