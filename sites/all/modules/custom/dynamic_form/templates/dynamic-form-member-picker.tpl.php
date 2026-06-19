<?php
/**
 * @file
 * Template for the AJAX user-search member picker widget.
 *
 * Variables:
 * - $roles:       Associative array of role_key => label for the role <select>.
 * - $placeholder: Already-translated placeholder string for the search input.
 * - $add_label:   Already-translated label for the Add button.
 */
?>
<div class="dfb-member-picker-wrap">
  <div class="dfb-member-input-row">
    <div class="dfb-member-search-wrap">
      <input type="text" id="dfb-members-search" class="dfb-member-search-input"
             placeholder="<?php print check_plain($placeholder); ?>" autocomplete="off" />
      <div id="dfb-members-results" class="dfb-members-results" style="display:none;"></div>
    </div>
    <select id="dfb-member-role-select" class="dfb-member-role-select">
      <?php foreach ($roles as $value => $label): ?>
        <option value="<?php print check_plain($value); ?>"><?php print check_plain($label); ?></option>
      <?php endforeach; ?>
    </select>
    <button type="button" id="dfb-member-add-btn" class="dfb-member-add-btn">
      <span class="dfb-add-btn-icon">+</span> <?php print check_plain($add_label); ?>
    </button>
  </div>
</div>
