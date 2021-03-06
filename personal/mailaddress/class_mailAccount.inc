<?php
/*!
  \brief   mail plugin
  \author  Mike Gabriel <mike.gabriel@das-netzwerkteam.de>
  \version 0.99.5
  \date    2015.08.27

  This class provides basic information on mail accounts. This plugin
  has been derived from gosa-plugin-mail, but it is not used to
  actually control mailservers. It is only used for allowing
  mail information to be stored in LDAP.

  This plugin actually breaks gosa-mail-account and is intended to be a
  drop-in replacement for it in situations where it is only necessary
  to store information in inetOrgPerson''s mail LDAP attribute description.

Functions :

 - mailAccount (&$config, $dn= NULL)
 - execute()
 - save_object()
 - remove_from_parent()
 - save()
 - check()
 - adapt_from_template($dn, $skip= array())
 - getCopyDialog()
 - saveCopyDialog()
 - multiple_execute()
 - multiple_check()
 - get_multi_edit_values () {
 - set_multi_edit_values ($attrs) {
 - plInfo()

 */

class mailAccount extends plugin {
    /* Definitions */
    var $plHeadline="Mail Address";
    var $plDescription="Manage user mail address";
    var $view_logged=FALSE;
    var $is_account=FALSE;
    var $initially_was_account=FALSE;

    /* GOsa mail attributes */
    var $mail="";

    /* GOsa mail account attributes filled with dummy values,
     * required due to gosaMailAccount objectclass requirements.
     */
    var $gosaMailServer="NULL";
    var $gosaMailDeliveryMode="[         ]";

    /* Used LDAP attributes && classes */
    var $attributes=array ("mail","gosaMailServer","gosaMailDeliveryMode",);
    var $objectclasses=array ("gosaMailAccount");
    var $multiple_support=TRUE;

    var $uid="";
    var $cn="";


    /*! \brief  Initialize the mailAccount
     */
    function __construct (&$config,$dn=NULL) {
        plugin::plugin ($config,$dn);

        /* Get attributes from parent object
         */
        foreach (array ("uid","cn") as $attr) {
            if (isset ($this->parent->by_object['group'])&&isset ($this->parent->by_object['group']->$attr)) {
                $this->$attr=&$this->parent->by_object['group']->$attr;
            }
            elseif (isset ($this->attrs[$attr])) {
                $this->$attr=$this->attrs[$attr][0];
            }
        }

        /* Remember account status
         */
        $this->initialy_was_account=$this->is_account;
    }

    function execute () {

        /* Call parent execute */
        $display=plugin::execute ();

        /* Log view */
        if ($this->is_account&&!$this->view_logged) {
            $this->view_logged=TRUE;
            new log ("view","users/".get_class ($this),$this->dn);
        }

        /****************
          Account status
         ****************/

        if (isset ($_POST['modify_state'])) {
            if ($this->is_account&&$this->acl_is_removeable ()) {
                $this->is_account=FALSE;
            }
            elseif (!$this->is_account&&$this->acl_is_createable ()) {
                $this->is_account=TRUE;
            }
        }

        if (!$this->multiple_support_active) {
            if (!$this->is_account&&$this->parent===NULL) {
                $display="<img alt=\"\" src=\"images/small-error.png\" align=\"middle\">&nbsp;<b>".msgPool::noValidExtension (_ ("Mail Address"))."</b>";
                $display.=back_to_main ();
                return ($display);
            }
            if ($this->parent!==NULL) {
                if ($this->is_account) {
                    $display=$this->show_disable_header (msgPool::removeFeaturesButton (_ ("Mail")),msgPool::featuresEnabled (_ ("Mail")));
                }
                else {
                    $display=$this->show_enable_header (msgPool::addFeaturesButton (_ ("Mail")),msgPool::featuresDisabled (_ ("Mail")));
                    return ($display);
                }
            }
        }

        /*    if(isset($_POST['edit_apply'])){
              if(isset($_POST['mail'])){
                  $this->mail = get_post('mail');
              }
            }
        */
        /****************
          SMARTY- Assign smarty variables
         ****************/
        $smarty=get_smarty ();

        $tmp=$this->plInfo ();
        foreach ($tmp['plProvidedAcls'] as $name=>$transl) {
            $smarty->assign ("$name"."ACL",$this->getacl ($name));
        }
        foreach ($this->attributes as $attr) {
            $smarty->assign ($attr,set_post ($this->$attr));
        }

        $smarty->assign ("multiple_support",$this->multiple_support_active);
        if ($this->multiple_support-active) {
            $smarty->assign ("multi_mail",array_slice ($this->multi_attrs_all['mail'],1));
        }
        return ($display.$smarty->fetch (get_template_path ("generic.tpl",TRUE,dirname (__FILE__))));
    }


    /* Save data to object */
    function save_object () {
        if (isset ($_POST['mailAccountTab'])) {

            /* Acquire updated mail address
             */
            if (isset ($_POST['mail'])) {
                $this->mail=get_post ('mail');
            }

            /* Save ldap attributes */
            $mail=$this->mail;
            $gosaMailServer=$this->gosaMailServer;
            $gosaMailDeliveryMode=$this->gosaMailDeliveryMode;
            plugin::save_object ();
            if ($this->initially_was_account&&!$this->is_template) {
                $this->mail=$mail;
                $this->gosaMailServer=$gosaMailServer;
                $this->gosaMailDeliveryMode=$gosaMailDeliveryMode;
            }
        }
    }


    /*! \brief  Removes the mailAccount extension from ldap
     */
    function remove_from_parent () {
        /* Cancel if there's nothing to do here */
        if (!$this->initially_was_account) {
            return;
        }

        // Do NOT remove the mail attribute while it is used in the Fax Account.
        if (isset ($this->parent->by_object['gofaxAccount'])) {
            $fax=$this->parent->by_object['gofaxAccount'];

            // Fax delivery to the mail account is activated, keep the mail attribute.
            if ($fax->goFaxDeliveryMode&32) {
                $this->attributes=array_remove_entries (array ('mail'),$this->attributes);
            }
        }

        /* Remove GOsa attributes */
        plugin::remove_from_parent ();

        $this->cleanup ();

        @DEBUG (DEBUG_LDAP,__LINE__,__FUNCTION__,__FILE__,$this->attributes,"Save");
        $ldap=$this->config->get_ldap_link ();
        $ldap->cd ($this->dn);
        $ldap->modify ($this->attrs);

        /* Add "view" to logging class */
        new log ("remove","users/".get_class ($this),$this->dn,array_keys ($this->attrs),$ldap->get_error ());
        if (!$ldap->success ()) {
            msg_dialog::display (_ ("LDAP error"),msgPool::ldaperror ($ldap->get_error (),$this->dn,LDAP_MOD,get_class ()));
        }

        /* Optionally execute a command after we are done */
        $this->handle_post_events ("remove",array ("uid"=>$this->uid));
    }


    /*! \brief  Save the mailAccount settings to the ldap database.
     */
    function save () {

        $ldap=$this->config->get_ldap_link ();

        if ($this->initially_was_account) {
            $this->mail=trim($this->mail);
        }

        /* Call parents save to prepare $this->attrs */
        plugin::save ();

        $this->attrs['mail']=$this->mail;
        $this->attrs['gosaMailServer']=$this->gosaMailServer;
        $this->attrs['gosaMailDeliveryMode']=$this->gosaMailDeliveryMode;

        /* Save data to LDAP */
        $ldap->cd ($this->dn);
        $this->cleanup ();
        $ldap->modify ($this->attrs);

        if (!$ldap->success ()) {
            msg_dialog::display (_ ("LDAP error"),msgPool::ldaperror ($ldap->get_error (),$this->dn,LDAP_MOD,get_class ()));
        }

        /* Log last action */
        if ($this->initially_was_account) {
            new log ("modify","users/".get_class ($this),$this->dn,array_keys ($this->attrs),$ldap->get_error ());
        }
        else {
            new log ("create","users/".get_class ($this),$this->dn,array_keys ($this->attrs),$ldap->get_error ());
        }

        /* Optionally execute a command after we are done */
        if ($this->initially_was_account==$this->is_account) {
            if ($this->is_modified) {
                $this->handle_post_events ("modify",array ("uid"=>$this->uid));
            }
        }
        else {
            $this->handle_post_events ("add",array ("uid"=>$this->uid));
        }
    }


    /*! \brief  Check given values
     */
    function check () {
        if (!$this->is_account) {
            return (array ());
        }

        $ldap=$this->config->get_ldap_link ();

        /* Call common method to give check the hook */
        $message=plugin::check ();

        /* Mail address checks */
        $mail=$this->mail;

        if (empty ($mail)) {
            if ($this->is_template) {
                $message[]=msgPool::required (_ ("Mail address"))."<br><br><b>"._ ("Examples").":</b> <i>{%givenName}.{%sn}@your-domain.com</i> "._ ("or")." <i>{%uid}@your-domain.com</i>";
            }
            else {
                $message[]=msgPool::required (_ ("Mail address"));
            }
        }

        if ($this->is_template) {
            if (!tests::is_email ($mail,TRUE)) {
                $message[]=msgPool::invalid (_ ("Mail address"),"","","{%givenName}.{%sn}@your-domain.com");
            }
        }
        else {
            if (!tests::is_email ($mail)) {
                $message[]=msgPool::invalid (_ ("Mail address"),"","","your-address@your-domain.com");
            }
        }

        /* Check if this mail address is already in use */
        $ldap->cd ($this->config->current['BASE']);
        $filter="(&(!(objectClass=gosaUserTemplate))(!(uid=".$this->uid."))"."(objectClass=gosaMailAccount)"."(|(mail=".$mail.")(alias=".$mail.")(gosaMailAlternateAddress=".$mail.")))";
        $ldap->search ($filter,array ("uid","cn"));
        if ($ldap->count ()!=0) {
            $entry=$ldap->fetch ();
            $addon="";
            if (!isset ($entry['uid'])) {
                $addon=sprintf (_ ("Address is already in use by group '%s'."),$entry['cn'][0]);
            }
            else {
                $addon=sprintf (_ ("Address is already in use by user '%s'."),$entry['uid'][0]);
            }
            $message[]=msgPool::duplicated (_ ("Mail address"))."<br><br><i>$addon</i>";
        }

        return ($message);
    }


    /*! \brief  Adapt from template, using 'dn'
     */
    function adapt_from_template ($dn,$skip=array ()) {
        plugin::adapt_from_template ($dn,$skip);

        $this->mail=rewrite ($this->mail);

        // Remove non ASCII charcters
        $this->mail=iconv ('UTF-8','US-ASCII//TRANSLIT',$this->mail);
        $this->mail=preg_replace ('/[^(\x20-\x7F)]*/','',$this->mail);

        // No spaces are allowed here
        $this->mail=preg_replace ("/[ ]/","",$this->mail);
    }


    /*! \brief  Creates the mail part for the copy & paste dialog
     */
    function getCopyDialog () {
        if (!$this->is_account)
            return ("");
        $smarty=get_smarty ();
        $smarty->assign ("mail",$this->mail);

        $str=$smarty->fetch (get_template_path ("copypaste.tpl",TRUE,dirname (__FILE__)));

        $ret=array ();
        $ret['status']="";
        $ret['string']=$str;
        return ($ret);
    }


    /*! \brief  save_object for copy&paste vars
     */
    function saveCopyDialog () {
        if (!$this->is_account)
            return;

        if (isset ($_POST['mail'])) {
            $this->mail=get_post ('mail');
        }
    }

    function multiple_execute () {
        return ($this->execute ());
    }

    function multiple_check () {
        $message=plugin::multiple_check ();

        if (empty ($this->gosaMailServer)&&in_array_strict ("gosaMailServer",$this->multi_boxes)) {
            $message[]=msgPool::noserver (_ ("Mail"));
        }

        /* Mail address checks */
        $mail_pattern=$this->mail;

        if (!empty ($mail_pattern)) {
            if ( (!tests::is_email ($mail_pattern,TRUE))|| (!preg_match ('/^.*({%uid}|{%givenName}|{%sn}|{%cn}).*@.*$/i',$mail_pattern))) {
                $message[]=msgPool::invalid (_ ("Mail address pattern"),"","","{%givenName}.{%sn}@your-domain.com")."<br><br><br>"._ ("Please make use of placeholders for the user part of the mail address.");
            }
        }
        return ($message);
    }

    function get_multi_edit_values () {
        $ret=plugin::get_multi_edit_values ();
        $ret['mail']=$this->mail;
        return $ret;
    }

    function set_multi_edit_values ($attrs) {
        /* Replace placeholders in mail address patterns */
        $mail_pattern=$attrs['mail'];

        if ($mail_pattern) {
            foreach (array ("sn","givenName","uid") as $repl) {
                $value=$this->$repl;
                if (preg_match ("/{%$repl}/",$mail_pattern)) {
                    $attrs['mail']=str_replace ("{%$repl}",$value,$mail_pattern);
                    $mail_pattern=$attrs['mail'];
                }
            }

            /* Add mail account data, if object is not in account state, yet
             */
            if (!$this->is_account&&$this->acl_is_createable ()) {
                $this->is_account=TRUE;
            }
        }
        else {
            $mail=$this->mail;
        }

        plugin::set_multi_edit_values ($attrs);

        if (!$mail_pattern) {
            $this->mail=$mail;
        }
    }


    /*! \brief  ACL settings
     */
    static function plInfo () {
        return (array ("plShortName"=>_ ("Mail"),"plDescription"=>_ ("User Mail Address"),"plSelfModify"=>TRUE,"plDepends"=>array ("user"),// This plugin depends on
        "plPriority"=>4,// Position in tabs
        "plSection"=>array ("personal"=>_ ("My account")),"plCategory"=>array ("users"),"plOptions"=>array (),"plRequirements"=>array ('onFailureDisablePlugin'=>array (get_class ())),"plProvidedAcls"=>array ("mail"=>_ ("Mail address"),),));
    }
}

// vim:tabstop=2:expandtab:shiftwidth=2:filetype=php:syntax:ruler:
?>
