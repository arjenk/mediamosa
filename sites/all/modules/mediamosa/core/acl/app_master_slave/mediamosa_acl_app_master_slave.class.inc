<?php
/**
 * @file
 * The acl_app_master_slave functions.
 */

/**
 * Master / slave;
 *
 * Giving other apps access to assets and mediafiles, you need to master / slave
 * apps. By making another app MASTER of your asset / mediafile you grant access
 * to that asset / mediafile. However; not only must the other (master) app have
 * access with master/slave, but also with the normal access rules like domain
 * and realm. As soon is_protected flag is true on the mediafile or asset, also
 * other rights have to be granted to have access to the master/slaved
 * asset/mediafile.
 *
 * Do not let the name master and slave confuse you. The master is not the app
 * that owns the asset or mediafile, but is the app that will be granted access.
 * The slave is the app of the asset/mediafile. The master is the app that has
 * been granted access to the asset/mediafile.
 */

class mediamosa_acl_app_master_slave {
  /**
   * Returns the join for joining with acl_app_master slave table.
   *
   * @param enum $acl_type
   *   See mediamosa_acl::ACL_TYPE_*.
   *
   * @return string
   *   The SQL join statement.
   */
  public static function join_acl_app_master_slave_get($acl_type) {
    switch ($acl_type) {
      case mediamosa_acl::ACL_TYPE_MEDIAFILE:
        $on = 'mf.mediafile_id';
        $alias = 'acl_ms';
        break;

      case mediamosa_acl::ACL_TYPE_COLLECTION:
        $on = 'c.coll_id';
        $alias = 'acl_msc';
        break;

      case mediamosa_acl::ACL_TYPE_ASSET:
        $on = 'a.asset_id';
        $alias = 'acl_msa';
        break;

      default:
        assert(0);
        return '';
    }
    return strtr(
      "LEFT JOIN {#acl_app_master_slave} AS #alias ON #alias.acl_object_type = '@acl_object_type' AND @on = #alias.acl_object_id",
      array(
        '#acl_app_master_slave' => mediamosa_acl_app_master_slave_db::TABLE_NAME,
        '@acl_object_type' => $acl_type,
        '@on' => $on,
        '#alias' => $alias,
      )
    );
  }

  /**
   * Create a master - slave record.
   *
   * @param string $acl_object_id
   *   The ACL object ID.
   * @param string $acl_object_type
   *   The ACL object type.
   * @param int $app_id_master
   *   The master app ID.
   * @param int $app_id_slave
   *   The slave app ID.
   * @param array $rights
   *   Unused for now.
   */
  public static function create($acl_object_id, $acl_object_type, $app_id_master, $app_id_slave, array $rights = array(mediamosa_acl::RIGHT_ACCESS)) {

    // Here to sadisify Zend about unused var.
    assert(is_array($rights));

    // Make sure we dont allow that app ids of master and slave are the same.
    if ($app_id_master == $app_id_slave) {
      throw new mediamosa_exception_error(mediamosa_error::ERRORCODE_MASTERSLAVE_OWN_APP);
    }

    // Check if we are allowed to master/slave to this app.
    $allowed_apps = mediamosa_app::get_allowed_master_slave_apps($app_id_master);

    // Is my app allowed to add?
    if (!in_array($app_id_slave, $allowed_apps)) {
      throw new mediamosa_exception_error_masterslave_disallowed(array('@acl_id' => $app_id_master));
    }

    // TODO: make mysql master/slave safe.
    try {
      mediamosa_db::db_query(
        'INSERT INTO {#mediamosa_acl_app_master_slave} SET #acl_object_type= :acl_object_type, #acl_object_id = :acl_object_id, #app_id_master = :app_id_master, #app_id_slave = :app_id_slave',
        array(
          '#mediamosa_acl_app_master_slave' => mediamosa_acl_app_master_slave_db::TABLE_NAME,
          '#acl_object_type' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_TYPE,
          ':acl_object_type' => $acl_object_type,
          '#acl_object_id' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_ID,
          ':acl_object_id' => $acl_object_id,
          '#app_id_master' => mediamosa_acl_app_master_slave_db::APP_ID_MASTER,
          ':app_id_master' => $app_id_master,
          '#app_id_slave' => mediamosa_acl_app_master_slave_db::APP_ID_SLAVE,
          ':app_id_slave' => $app_id_slave
        )
      );
    }
    catch (PDOException $e) {
      assert($e);
      // ignore, double row.
    }

    switch ($acl_object_type) {
      case mediamosa_acl_app_master_slave_db::ACL_OBJECT_TYPE_ASSET:
        // Trigger the reindex hook for external search engines.
        mediamosa_asset::mediamosa_asset_reindex(array($acl_object_id), mediamosa_settings::SEARCH_INDEX_TYPE_MASTER_SLAVE);
        break;

      case mediamosa_acl_app_master_slave_db::ACL_OBJECT_TYPE_MEDIAFILE:
        // Trigger the reindex hook for external search engines.
        mediamosa_asset_mediafile::mediamosa_asset_reindex(array($acl_object_id), mediamosa_settings::SEARCH_INDEX_TYPE_MASTER_SLAVE);
        break;
    }
  }

  /**
   * Return the app_id(s) of EGAs that have slaves of my supplied EGA (master).
   *
   * @param array/integer $mixed_app_id
   *   The app ID array or integer.
   * @param string $acl_object_type
   *   The type of acl (mediamosa_acl::ACL_TYPE_*).
   * @param string $object_id
   *   (optional) The ID of object you want to know slaves on.
   *
   * @return array
   *   returns array with info about the slaves.
   */
  public static function slave_get($mixed_app_id, $acl_object_type, $object_id = NULL) {
    $app_ids = is_array($mixed_app_id) ? $mixed_app_id : array($mixed_app_id);
    if (!count($app_ids)) {
      return array();
    }

    if (isset($object_id)) {
      $result = mediamosa_db::db_query(
        'SELECT #app_id_slave FROM {#mediamosa_acl_app_master_slave} WHERE #acl_object_type = :acl_object_type AND #acl_object_id = :acl_object_id AND #app_id_master IN(:app_id_master) GROUP BY #app_id_slave',
        array(
          '#app_id_slave' => mediamosa_acl_app_master_slave_db::APP_ID_SLAVE,
          '#mediamosa_acl_app_master_slave' => mediamosa_acl_app_master_slave_db::TABLE_NAME,
          '#acl_object_type' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_TYPE,
          ':acl_object_type' => $acl_object_type,
          '#acl_object_id' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_ID,
          ':acl_object_id' => $object_id,
          '#app_id_master' => mediamosa_acl_app_master_slave_db::APP_ID_MASTER,
          ':app_id_master' => $app_ids,
        )
      );
    }
    else {
      $result = mediamosa_db::db_query(
        'SELECT #app_id_slave FROM {#mediamosa_acl_app_master_slave} WHERE #acl_object_type = :acl_object_type AND #app_id_master IN(:app_id_master) GROUP BY #app_id_slave',
        array(
          '#app_id_slave' => mediamosa_acl_app_master_slave_db::APP_ID_SLAVE,
          '#mediamosa_acl_app_master_slave' => mediamosa_acl_app_master_slave_db::TABLE_NAME,
          '#acl_object_type' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_TYPE,
          ':acl_object_type' => $acl_object_type,
          '#app_id_master' => mediamosa_acl_app_master_slave_db::APP_ID_MASTER,
          ':app_id_master' => $app_ids,
        )
      );
    }

    $slave_apps = array();
    foreach ($result as $row) {
      $slave_apps[$row[mediamosa_acl_app_master_slave_db::APP_ID_SLAVE]] = array('app_id' => $row[mediamosa_acl_app_master_slave_db::APP_ID_SLAVE]);
    }

    return $slave_apps;
  }

  /**
   * Return the master app_id  of my supplied slave app.
   *
   * @param int $app_id_slave
   *   The slave app ID.
   * @param string $acl_object_type
   *   The type of acl (mediamosa_acl::ACL_TYPE_*).
   * @param string $acl_object_id
   *   (optional) The ID of object you want to know slaves on.
   *
   * @return int
   *   returns master app_id.
   */
  public static function master_get($app_id_slave, $acl_object_type, $acl_object_id) {

    static $a_app_id_masters = array();

    if (isset($a_app_id_masters[$app_id_slave][$acl_object_type][$acl_object_id])) {
      return $a_app_id_masters[$app_id_slave][$acl_object_type][$acl_object_id];
    }

    $result = mediamosa_db::db_query(
      'SELECT #app_id_master FROM {#acl_app_master_slave} WHERE #acl_object_type = :acl_object_type AND #acl_object_id = :acl_object_id AND #app_id_master > 0 AND #app_id_slave = :app_id_slave',
      array(
        '#acl_app_master_slave' => mediamosa_acl_app_master_slave_db::TABLE_NAME,
        '#acl_object_type' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_TYPE,
        ':acl_object_type' => $acl_object_type,
        '#acl_object_id' => mediamosa_acl_app_master_slave_db::ACL_OBJECT_ID,
        ':acl_object_id' => $acl_object_id,
        '#app_id_master' => mediamosa_acl_app_master_slave_db::APP_ID_MASTER,
        '#app_id_slave' => mediamosa_acl_app_master_slave_db::APP_ID_SLAVE,
        ':app_id_slave' => $app_id_slave
      )
    )->fetchAll();

    // Default none.
    $a_app_id_masters[$app_id_slave][$acl_object_type][$acl_object_id] = array();

    // Fill it.
    foreach ($result as $row) {
      $a_app_id_masters[$app_id_slave][$acl_object_type][$acl_object_id][] = $row[mediamosa_acl_app_master_slave_db::APP_ID_MASTER];
    }

    // Return possible masters.
    return $a_app_id_masters[$app_id_slave][$acl_object_type][$acl_object_id];
  }
}
